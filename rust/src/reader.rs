// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    free_handle,
    header::LasrsHeader,
    into_handle, limits,
    point_data::LasrsPointData,
    stream::{InputStream, LasrsInputStream},
    types::LasrsStr,
};
use las::{Header, LazParallelism, Reader, ReaderOptions};
use std::{
    fs::File,
    io::{BufReader, Read, Seek},
};

/// Keeps its own copy of the header so the pointer handed out by
/// `lasrs_reader_header` never aliases the mutably borrowed reader.
pub struct LasrsReader {
    reader: Reader,
    header: Header,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum LasrsLazParallelism {
    Yes = 0,
    No = 1,
}

impl From<LasrsLazParallelism> for LazParallelism {
    fn from(p: LasrsLazParallelism) -> Self {
        match p {
            LasrsLazParallelism::Yes => LazParallelism::Yes,
            LasrsLazParallelism::No => LazParallelism::No,
        }
    }
}

fn open<R: Read + Seek + Send + Sync + 'static>(
    out: *mut *mut LasrsReader,
    source: impl FnOnce() -> Result<R, BoxError>,
    options: ReaderOptions,
) -> LasrsStatus {
    guard(|| {
        let mut read = source()?;
        limits::check_declared_sizes(&mut read)?;
        let reader = Reader::with_options(read, options)?;
        let header = reader.header().clone();
        unsafe { out.write(into_handle(LasrsReader { reader, header })) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_from_path(
    path: LasrsStr,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    let source = || Ok(BufReader::new(File::open(unsafe { path.to_str() }?)?));
    open(out, source, ReaderOptions::default())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_new(
    stream: LasrsInputStream,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    open(
        out,
        || Ok(InputStream::buffered(stream)),
        ReaderOptions::default(),
    )
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_with_options(
    stream: LasrsInputStream,
    laz_parallelism: LasrsLazParallelism,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    let options = ReaderOptions::default().with_laz_parallelism(laz_parallelism.into());
    open(out, || Ok(InputStream::buffered(stream)), options)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_free(reader: *mut LasrsReader) {
    unsafe { free_handle(reader) }
}

/// Borrowed, valid while the reader is alive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_header(reader: *const LasrsReader) -> *const LasrsHeader {
    LasrsHeader::borrow(unsafe { &(*reader).header })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_read_points(
    reader: *mut LasrsReader,
    n: u64,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let points = limits::read_points(unsafe { &mut (*reader).reader }, n)?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_read_all(
    reader: *mut LasrsReader,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let reader = unsafe { &mut (*reader).reader };
        let remaining = reader.header().number_of_points();
        let points = limits::read_points(reader, remaining)?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_fill_points(
    reader: *mut LasrsReader,
    n: u64,
    target: *mut LasrsPointData,
    read: *mut u64,
) -> LasrsStatus {
    guard(|| {
        let count = unsafe { &mut (*reader).reader }.fill_points(n, unsafe { &mut (*target).0 })?;
        unsafe { read.write(count) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_seek(reader: *mut LasrsReader, position: u64) -> LasrsStatus {
    guard(|| {
        let reader = unsafe { &mut (*reader).reader };
        // las-rs underflows when reading after a seek past the last point.
        let position = position.min(reader.header().number_of_points());
        Ok(reader.seek(position)?)
    })
}
