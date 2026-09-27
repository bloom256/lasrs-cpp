// SPDX-License-Identifier: MIT OR Apache-2.0

//! las-rs allocates buffers from sizes declared in the file before reading
//! the data. A failed allocation aborts the process, so a corrupt or hostile
//! file must not reach those allocations: declared sizes are checked
//! against the real stream length first, and point reads go in batches so
//! memory follows the data actually present.

use crate::error::BoxError;
use las::{PointData, PointDataBuilder, Reader, raw};
use std::io::{Read, Seek, SeekFrom};

const BATCH_POINTS: u64 = 1 << 20;
const EVLR_HEADER_SIZE: u64 = 60;
const EVLR_RECORD_LENGTH_OFFSET: u64 = 20;

/// Checks that the EVLRs declared in the header lie inside the stream.
/// Leaves the stream where it was.
pub(crate) fn check_declared_sizes<R: Read + Seek>(read: &mut R) -> Result<(), BoxError> {
    let start = read.stream_position()?;
    let end = read.seek(SeekFrom::End(0))?;
    read.seek(SeekFrom::Start(start))?;
    let header = raw::Header::read_from(&mut *read);
    let result = header
        .map_err(BoxError::from)
        .and_then(|header| match header.evlr {
            Some(evlr) => check_evlrs(read, evlr, end),
            None => Ok(()),
        });
    read.seek(SeekFrom::Start(start))?;
    result
}

fn check_evlrs<R: Read + Seek>(
    read: &mut R,
    evlr: raw::header::Evlr,
    end: u64,
) -> Result<(), BoxError> {
    let past_end = || {
        format!("the EVLRs declared in the header extend past the end of the data ({end} bytes)")
    };
    let mut position = evlr.start_of_first_evlr;
    if position > end {
        return Err(past_end().into());
    }
    for _ in 0..evlr.number_of_evlrs {
        read.seek(SeekFrom::Start(position + EVLR_RECORD_LENGTH_OFFSET))?;
        let mut record_length = [0; 8];
        read.read_exact(&mut record_length)?;
        position = position
            .checked_add(EVLR_HEADER_SIZE)
            .and_then(|p| p.checked_add(u64::from_le_bytes(record_length)))
            .filter(|&p| p <= end)
            .ok_or_else(past_end)?;
    }
    Ok(())
}

/// Reads up to `n` points in batches.
pub(crate) fn read_points(reader: &mut Reader, n: u64) -> Result<PointData, BoxError> {
    let builder = PointDataBuilder::new().for_header(reader.header());
    let mut batch = builder.clone().build();
    let mut bytes = Vec::new();
    let mut remaining = n;
    while remaining > 0 {
        let read = reader.fill_points(remaining.min(BATCH_POINTS), &mut batch)?;
        if read == 0 {
            break;
        }
        bytes.extend_from_slice(batch.raw_bytes());
        remaining -= read;
    }
    Ok(builder.build_from_bytes(bytes)?)
}
