// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    header::{LasrsHeader, header_ref},
    into_handle,
    point_data::LasrsPointData,
    reader::LasrsLazParallelism,
    stream::{LasrsOutputStream, OutputStream},
    types::{LasrsPoint, LasrsStr, borrow_slice},
};
use las::{Header, Writer, WriterOptions};
use std::{fs::File, io::BufWriter};

pub enum LasrsWriter {
    File(Writer<BufWriter<File>>),
    Stream(Writer<OutputStream>),
}

macro_rules! with_writer {
    ($handle:expr, $w:ident => $body:expr) => {
        match unsafe { &mut *$handle } {
            LasrsWriter::File($w) => $body,
            LasrsWriter::Stream($w) => $body,
        }
    };
}

fn open(
    out: *mut *mut LasrsWriter,
    make: impl FnOnce() -> Result<LasrsWriter, BoxError>,
) -> LasrsStatus {
    guard(|| {
        let writer = make()?;
        unsafe { out.write(into_handle(writer)) };
        Ok(())
    })
}

/// # Safety
/// `header` must be a valid handle.
unsafe fn owned_header(header: *const LasrsHeader) -> Header {
    unsafe { header_ref(header) }.clone()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_from_path(
    path: LasrsStr,
    header: *const LasrsHeader,
    out: *mut *mut LasrsWriter,
) -> LasrsStatus {
    open(out, || {
        let path = unsafe { path.to_str() }?;
        let header = unsafe { owned_header(header) };
        Ok(LasrsWriter::File(Writer::from_path(path, header)?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_new(
    stream: LasrsOutputStream,
    header: *const LasrsHeader,
    out: *mut *mut LasrsWriter,
) -> LasrsStatus {
    open(out, || {
        let header = unsafe { owned_header(header) };
        Ok(LasrsWriter::Stream(Writer::new(
            OutputStream::new(stream),
            header,
        )?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_with_options(
    stream: LasrsOutputStream,
    header: *const LasrsHeader,
    laz_parallelism: LasrsLazParallelism,
    out: *mut *mut LasrsWriter,
) -> LasrsStatus {
    open(out, || {
        let header = unsafe { owned_header(header) };
        let options = WriterOptions::default().with_laz_parallelism(laz_parallelism.into());
        Ok(LasrsWriter::Stream(Writer::with_options(
            OutputStream::new(stream),
            header,
            options,
        )?))
    })
}

/// Closes the writer if still open. Errors while closing are reported
/// through the status, the handle is freed either way.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_free(writer: *mut LasrsWriter) -> LasrsStatus {
    if writer.is_null() {
        return LasrsStatus::Ok;
    }
    let writer = unsafe { Box::from_raw(writer) };
    guard(move || {
        drop(writer);
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_close(writer: *mut LasrsWriter) -> LasrsStatus {
    guard(|| with_writer!(writer, w => Ok(w.close()?)))
}

/// Returns a new owned copy of the writer's current header.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_header(writer: *mut LasrsWriter) -> *mut LasrsHeader {
    with_writer!(writer, w => LasrsHeader::new_handle(w.header().clone()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_write_point(
    writer: *mut LasrsWriter,
    point: *const LasrsPoint,
    extra_bytes: *const u8,
) -> LasrsStatus {
    guard(|| {
        let point = unsafe { *point };
        let extra = unsafe { borrow_slice(extra_bytes, point.extra_bytes_len) };
        let point = point.to_point(extra)?;
        with_writer!(writer, w => Ok(w.write_point(point)?))
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_writer_write_points(
    writer: *mut LasrsWriter,
    points: *const LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let points = unsafe { &(*points).0 };
        with_writer!(writer, w => Ok(w.write_points(points)?))
    })
}
