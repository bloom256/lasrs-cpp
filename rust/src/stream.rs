// SPDX-License-Identifier: MIT OR Apache-2.0

use std::{
    ffi::c_void,
    io::{self, BufReader, BufWriter, Read, Seek, SeekFrom, Write},
};

/// Origin of a seek, as in `std::io::SeekFrom`.
#[repr(C)]
#[derive(Clone, Copy)]
pub enum LasrsSeekOrigin {
    Start = 0,
    Current = 1,
    End = 2,
}

/// Reads up to `len` bytes into `buf`, storing the count in `read`.
/// Returns false on error. Must not throw or unwind.
pub type LasrsReadFn =
    unsafe extern "C" fn(context: *mut c_void, buf: *mut u8, len: usize, read: *mut usize) -> bool;

/// Writes all `len` bytes of `buf`. Returns false on error.
pub type LasrsWriteFn =
    unsafe extern "C" fn(context: *mut c_void, buf: *const u8, len: usize) -> bool;

/// Seeks and stores the new absolute position in `position`.
pub type LasrsSeekFn = unsafe extern "C" fn(
    context: *mut c_void,
    offset: i64,
    origin: LasrsSeekOrigin,
    position: *mut u64,
) -> bool;

/// Flushes buffered output. Returns false on error.
pub type LasrsFlushFn = unsafe extern "C" fn(context: *mut c_void) -> bool;

/// A caller-implemented input stream. `context` must outlive every handle
/// created from it and must not be used concurrently.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsInputStream {
    pub context: *mut c_void,
    pub read: LasrsReadFn,
    pub seek: LasrsSeekFn,
}

/// A caller-implemented output stream, same rules as [`LasrsInputStream`].
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsOutputStream {
    pub context: *mut c_void,
    pub write: LasrsWriteFn,
    pub seek: LasrsSeekFn,
    pub flush: LasrsFlushFn,
}

pub(crate) struct InputStream(LasrsInputStream);
pub(crate) struct OutputStream(LasrsOutputStream);

/// Buffered like las-rs buffers files in `from_path`: the sequential LAZ
/// codec issues many tiny reads and writes, each of which would otherwise
/// cross into C++.
pub(crate) type BufferedInput = BufReader<InputStream>;
pub(crate) type BufferedOutput = BufWriter<OutputStream>;

impl InputStream {
    pub(crate) fn buffered(stream: LasrsInputStream) -> BufferedInput {
        BufReader::new(Self(stream))
    }
}

impl OutputStream {
    pub(crate) fn buffered(stream: LasrsOutputStream) -> BufferedOutput {
        BufWriter::new(Self(stream))
    }
}

// SAFETY: the C API contract requires the context to outlive the handle and
// to be used from one thread at a time, which `&mut self` access guarantees.
unsafe impl Send for InputStream {}
unsafe impl Sync for InputStream {}
unsafe impl Send for OutputStream {}
unsafe impl Sync for OutputStream {}

fn stream_error(operation: impl std::fmt::Display) -> io::Error {
    io::Error::other(format!("stream {operation} failed"))
}

fn seek_with(seek: LasrsSeekFn, context: *mut c_void, position: SeekFrom) -> io::Result<u64> {
    let (offset, origin) = match position {
        SeekFrom::Start(n) => (
            i64::try_from(n).map_err(|_| stream_error(format_args!("seek to {position:?}")))?,
            LasrsSeekOrigin::Start,
        ),
        SeekFrom::Current(n) => (n, LasrsSeekOrigin::Current),
        SeekFrom::End(n) => (n, LasrsSeekOrigin::End),
    };
    let mut new_position = 0;
    if unsafe { seek(context, offset, origin, &mut new_position) } {
        Ok(new_position)
    } else {
        Err(stream_error(format_args!("seek to {position:?}")))
    }
}

impl Read for InputStream {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let mut read = 0;
        if unsafe { (self.0.read)(self.0.context, buf.as_mut_ptr(), buf.len(), &mut read) } {
            Ok(read)
        } else {
            Err(stream_error(format_args!("read of {} bytes", buf.len())))
        }
    }
}

impl Seek for InputStream {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        seek_with(self.0.seek, self.0.context, position)
    }
}

impl Write for OutputStream {
    fn write(&mut self, buf: &[u8]) -> io::Result<usize> {
        if unsafe { (self.0.write)(self.0.context, buf.as_ptr(), buf.len()) } {
            Ok(buf.len())
        } else {
            Err(stream_error(format_args!("write of {} bytes", buf.len())))
        }
    }

    fn flush(&mut self) -> io::Result<()> {
        if unsafe { (self.0.flush)(self.0.context) } {
            Ok(())
        } else {
            Err(stream_error("flush"))
        }
    }
}

impl Seek for OutputStream {
    fn seek(&mut self, position: SeekFrom) -> io::Result<u64> {
        seek_with(self.0.seek, self.0.context, position)
    }
}
