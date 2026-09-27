// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    free_handle,
    header::{LasrsHeader, header_ref},
    into_handle, limits,
    point_data::LasrsPointData,
    stream::{BufferedInput, InputStream, LasrsInputStream},
    types::{LasrsBounds, LasrsEntry, LasrsStr, LasrsVoxelKey, borrow_slice_mut},
};
use las::{
    BoundsSelection, CopcReader, Header, LodSelection,
    copc::{CopcHierarchyVlr, Entry, VoxelKey},
};
use std::{fs::File, io::BufReader};

enum Source {
    File(CopcReader<'static, BufReader<File>>),
    Stream(CopcReader<'static, BufferedInput>),
}

/// Keeps its own copy of the header so the pointer handed out by
/// `lasrs_copc_reader_header` never aliases the mutably borrowed reader.
pub struct LasrsCopcReader {
    source: Source,
    header: Header,
}

macro_rules! with_reader {
    ($handle:expr, $r:ident => $body:expr) => {
        match $handle {
            Source::File($r) => $body,
            Source::Stream($r) => $body,
        }
    };
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum LasrsLodSelectionKind {
    All = 0,
    Resolution = 1,
    Level = 2,
    LevelMinMax = 3,
}

/// Only the fields used by `kind` are read.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsLodSelection {
    pub kind: LasrsLodSelectionKind,
    pub resolution: f64,
    pub level: i32,
    pub level_max: i32,
}

impl From<LasrsLodSelection> for LodSelection {
    fn from(s: LasrsLodSelection) -> Self {
        match s.kind {
            LasrsLodSelectionKind::All => LodSelection::All,
            LasrsLodSelectionKind::Resolution => LodSelection::Resolution(s.resolution),
            LasrsLodSelectionKind::Level => LodSelection::Level(s.level),
            LasrsLodSelectionKind::LevelMinMax => LodSelection::LevelMinMax(s.level, s.level_max),
        }
    }
}

/// `bounds` is only read when `within` is true.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsBoundsSelection {
    pub within: bool,
    pub bounds: LasrsBounds,
}

impl From<LasrsBoundsSelection> for BoundsSelection {
    fn from(s: LasrsBoundsSelection) -> Self {
        if s.within {
            BoundsSelection::Within(s.bounds.into())
        } else {
            BoundsSelection::All
        }
    }
}

fn open(
    out: *mut *mut LasrsCopcReader,
    make: impl FnOnce() -> Result<Source, BoxError>,
) -> LasrsStatus {
    guard(|| {
        let source = make()?;
        let header = with_reader!(&source, r => r.header().clone());
        unsafe { out.write(into_handle(LasrsCopcReader { source, header })) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_from_path(
    path: LasrsStr,
    out: *mut *mut LasrsCopcReader,
) -> LasrsStatus {
    open(out, || {
        let mut read = BufReader::new(File::open(unsafe { path.to_str() }?)?);
        limits::check_declared_sizes(&mut read)?;
        Ok(Source::File(CopcReader::new(read)?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_new(
    stream: LasrsInputStream,
    out: *mut *mut LasrsCopcReader,
) -> LasrsStatus {
    open(out, || {
        let mut read = InputStream::buffered(stream);
        limits::check_declared_sizes(&mut read)?;
        Ok(Source::Stream(CopcReader::new(read)?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_free(reader: *mut LasrsCopcReader) {
    unsafe { free_handle(reader) }
}

/// Borrowed, valid while the reader is alive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_header(
    reader: *const LasrsCopcReader,
) -> *const LasrsHeader {
    LasrsHeader::borrow(unsafe { &(*reader).header })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_hierarchy_entries_len(
    reader: *const LasrsCopcReader,
) -> usize {
    with_reader!(unsafe { &(*reader).source }, r => r.hierarchy_entries().count())
}

/// Writes up to `capacity` entries to `out`; returns the number written.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_hierarchy_entries(
    reader: *const LasrsCopcReader,
    out: *mut LasrsEntry,
    capacity: usize,
) -> usize {
    let out = unsafe { borrow_slice_mut(out, capacity) };
    with_reader!(unsafe { &(*reader).source }, r => {
        out.iter_mut()
            .zip(r.hierarchy_entries())
            .map(|(slot, entry)| *slot = entry.into())
            .count()
    })
}

/// Returns false if there is no entry for `key`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_hierarchy_entry(
    reader: *const LasrsCopcReader,
    key: LasrsVoxelKey,
    out: *mut LasrsEntry,
) -> bool {
    let key = VoxelKey::from(key);
    match with_reader!(unsafe { &(*reader).source }, r => r.hierarchy_entry(&key)) {
        Some(entry) => {
            unsafe { out.write(entry.into()) };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_read_entry(
    reader: *mut LasrsCopcReader,
    entry: LasrsEntry,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let entry = Entry::from(entry);
        let source = unsafe { &mut (*reader).source };
        // las-rs allocates the entry's declared point count; only accept
        // entries that really are in this file's hierarchy.
        let known = with_reader!(&*source, r => r.hierarchy_entry(&entry.key));
        let matches = known.is_some_and(|known| {
            (known.offset, known.byte_size, known.point_count)
                == (entry.offset, entry.byte_size, entry.point_count)
        });
        if !matches {
            return Err("the entry is not part of this file's COPC hierarchy".into());
        }
        let points = with_reader!(source, r => r.read_entry(&entry))?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_query(
    reader: *mut LasrsCopcReader,
    levels: LasrsLodSelection,
    bounds: LasrsBoundsSelection,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let (levels, bounds) = (levels.into(), bounds.into());
        let points = with_reader!(unsafe { &mut (*reader).source }, r => r.query(levels, bounds))?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

/// Opaque handle; owns a `las::copc::CopcHierarchyVlr`.
pub struct LasrsCopcHierarchyVlr(CopcHierarchyVlr);

/// Returns null if the header has no readable COPC hierarchy EVLR.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_copc_hierarchy_evlr(
    header: *const LasrsHeader,
) -> *mut LasrsCopcHierarchyVlr {
    unsafe { header_ref(header) }
        .copc_hierarchy_evlr()
        .map_or(std::ptr::null_mut(), |vlr| {
            into_handle(LasrsCopcHierarchyVlr(vlr))
        })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_hierarchy_vlr_clone(
    vlr: *const LasrsCopcHierarchyVlr,
) -> *mut LasrsCopcHierarchyVlr {
    into_handle(LasrsCopcHierarchyVlr(unsafe { &(*vlr).0 }.clone()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_hierarchy_vlr_free(vlr: *mut LasrsCopcHierarchyVlr) {
    unsafe { free_handle(vlr) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_hierarchy_vlr_entries_len(
    vlr: *const LasrsCopcHierarchyVlr,
    out: *mut usize,
) -> LasrsStatus {
    guard(|| {
        let mut len = 0;
        for entry in unsafe { &(*vlr).0 }.iter_entries() {
            let _ = entry?;
            len += 1;
        }
        unsafe { out.write(len) };
        Ok(())
    })
}

/// Writes up to `capacity` entries of `iter_entries` to `out` and the number
/// written to `written`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_hierarchy_vlr_iter_entries(
    vlr: *const LasrsCopcHierarchyVlr,
    out: *mut LasrsEntry,
    capacity: usize,
    written: *mut usize,
) -> LasrsStatus {
    guard(|| {
        let out = unsafe { borrow_slice_mut(out, capacity) };
        let mut n = 0;
        for (slot, entry) in out.iter_mut().zip(unsafe { &(*vlr).0 }.iter_entries()) {
            *slot = (*entry?).into();
            n += 1;
        }
        unsafe { written.write(n) };
        Ok(())
    })
}
