// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    free_handle,
    header::LasrsHeader,
    into_handle,
    point_data::LasrsPointData,
    stream::{InputStream, LasrsInputStream},
    types::{LasrsBounds, LasrsEntry, LasrsStr, LasrsVoxelKey, borrow_slice_mut},
};
use las::{BoundsSelection, CopcReader, LodSelection, copc::VoxelKey};
use std::{fs::File, io::BufReader};

pub enum LasrsCopcReader {
    File(CopcReader<'static, BufReader<File>>),
    Stream(CopcReader<'static, InputStream>),
}

macro_rules! with_reader {
    ($handle:expr, $r:ident => $body:expr) => {
        match $handle {
            LasrsCopcReader::File($r) => $body,
            LasrsCopcReader::Stream($r) => $body,
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
    make: impl FnOnce() -> Result<LasrsCopcReader, BoxError>,
) -> LasrsStatus {
    guard(|| {
        let reader = make()?;
        unsafe { out.write(into_handle(reader)) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_from_path(
    path: LasrsStr,
    out: *mut *mut LasrsCopcReader,
) -> LasrsStatus {
    open(out, || {
        let path = unsafe { path.to_str() }?;
        Ok(LasrsCopcReader::File(CopcReader::from_path(path)?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_new(
    stream: LasrsInputStream,
    out: *mut *mut LasrsCopcReader,
) -> LasrsStatus {
    open(out, || {
        Ok(LasrsCopcReader::Stream(CopcReader::new(InputStream::new(
            stream,
        ))?))
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
    with_reader!(unsafe { &*reader }, r => LasrsHeader::borrow(r.header()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_hierarchy_entries_len(
    reader: *const LasrsCopcReader,
) -> usize {
    with_reader!(unsafe { &*reader }, r => r.hierarchy_entries().count())
}

/// Writes up to `capacity` entries to `out`; returns the number written.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_copc_reader_hierarchy_entries(
    reader: *const LasrsCopcReader,
    out: *mut LasrsEntry,
    capacity: usize,
) -> usize {
    let out = unsafe { borrow_slice_mut(out, capacity) };
    with_reader!(unsafe { &*reader }, r => {
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
    match with_reader!(unsafe { &*reader }, r => r.hierarchy_entry(&key)) {
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
        let entry = entry.into();
        let points = with_reader!(unsafe { &mut *reader }, r => r.read_entry(&entry))?;
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
        let points = with_reader!(unsafe { &mut *reader }, r => r.query(levels, bounds))?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}
