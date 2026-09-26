// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    free_handle,
    header::LasrsHeader,
    into_handle,
    point_data::LasrsPointData,
    stream::{InputStream, LasrsInputStream},
    types::LasrsStr,
};
use las::{LazParallelism, Reader, ReaderOptions};

pub struct LasrsReader(Reader);

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

fn open(
    out: *mut *mut LasrsReader,
    make: impl FnOnce() -> Result<Reader, BoxError>,
) -> LasrsStatus {
    guard(|| {
        let reader = make()?;
        unsafe { out.write(into_handle(LasrsReader(reader))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_from_path(
    path: LasrsStr,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    open(out, || Ok(Reader::from_path(unsafe { path.to_str() }?)?))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_new(
    stream: LasrsInputStream,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    open(out, || Ok(Reader::new(InputStream::buffered(stream))?))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_with_options(
    stream: LasrsInputStream,
    laz_parallelism: LasrsLazParallelism,
    out: *mut *mut LasrsReader,
) -> LasrsStatus {
    open(out, || {
        let options = ReaderOptions::default().with_laz_parallelism(laz_parallelism.into());
        Ok(Reader::with_options(
            InputStream::buffered(stream),
            options,
        )?)
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_free(reader: *mut LasrsReader) {
    unsafe { free_handle(reader) }
}

/// Borrowed, valid while the reader is alive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_header(reader: *const LasrsReader) -> *const LasrsHeader {
    LasrsHeader::borrow(unsafe { &(*reader).0 }.header())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_read_points(
    reader: *mut LasrsReader,
    n: u64,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let points = unsafe { &mut (*reader).0 }.read_points(n)?;
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
        let points = unsafe { &mut (*reader).0 }.read_all()?;
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
        let count = unsafe { &mut (*reader).0 }.fill_points(n, unsafe { &mut (*target).0 })?;
        unsafe { read.write(count) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_reader_seek(reader: *mut LasrsReader, position: u64) -> LasrsStatus {
    guard(|| Ok(unsafe { &mut (*reader).0 }.seek(position)?))
}
