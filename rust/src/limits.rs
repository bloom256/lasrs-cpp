// SPDX-License-Identifier: MIT OR Apache-2.0

//! las-rs allocates buffers from sizes declared in the file before reading
//! the data. A failed allocation aborts the process, so a corrupt or hostile
//! file must not reach those allocations: declared sizes are checked
//! against the real stream length first, and point reads go in batches so
//! memory follows the data actually present.
//!
//! What remains is las-rs allocating the points of a COPC hierarchy entry
//! from its declared point count; LAZ has no bound on points per byte.

use crate::error::BoxError;
use las::{PointData, PointDataBuilder, Reader, raw};
use std::io::{Read, Seek, SeekFrom};

const BATCH_POINTS: u64 = 1 << 20;
const EVLR_HEADER_SIZE: u64 = 60;
const EVLR_RECORD_LENGTH_OFFSET: u64 = 20;
// Every LAZ chunk starts with its first point stored raw, and the smallest
// point record is 20 bytes.
const MIN_LAZ_CHUNK_SIZE: u64 = 20;

/// Checks that the EVLRs declared in the header lie inside the stream and
/// that the LAZ chunk table does not declare more chunks than fit into the
/// compressed data. Leaves the stream where it was.
pub(crate) fn check_declared_sizes<R: Read + Seek>(read: &mut R) -> Result<(), BoxError> {
    let start = read.stream_position()?;
    let end = read.seek(SeekFrom::End(0))?;
    read.seek(SeekFrom::Start(start))?;
    let result = raw::Header::read_from(&mut *read)
        .map_err(BoxError::from)
        .and_then(|header| {
            if let Some(evlr) = header.evlr {
                check_evlrs(read, evlr, end)?;
            }
            if header.point_data_record_format & 0x80 != 0 {
                check_laz_chunk_count(read, header.offset_to_point_data.into(), end)?;
            }
            Ok(())
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

/// Finds the chunk table as laz-rs does: its offset is stored at the start
/// of the point data, or, if the writer could not update it, in the last
/// 8 bytes. An offset laz-rs would reject is left to laz-rs.
fn check_laz_chunk_count<R: Read + Seek>(
    read: &mut R,
    data_start: u64,
    end: u64,
) -> Result<(), BoxError> {
    let mut offset = [0; 8];
    read.seek(SeekFrom::Start(data_start))?;
    read.read_exact(&mut offset)?;
    let mut chunk_table = i64::from_le_bytes(offset);
    if chunk_table <= data_start as i64 {
        read.seek(SeekFrom::End(-8))?;
        read.read_exact(&mut offset)?;
        chunk_table = i64::from_le_bytes(offset);
    }
    let Ok(chunk_table) = u64::try_from(chunk_table) else {
        return Ok(());
    };
    if chunk_table <= data_start || chunk_table.saturating_add(8) > end {
        return Ok(());
    }
    read.seek(SeekFrom::Start(chunk_table + 4))?;
    let mut count = [0; 4];
    read.read_exact(&mut count)?;
    let chunks = u64::from(u32::from_le_bytes(count));
    let data_size = chunk_table - data_start;
    if chunks.saturating_mul(MIN_LAZ_CHUNK_SIZE) > data_size {
        return Err(format!(
            "the LAZ chunk table declares {chunks} chunks, more than {data_size} bytes of              compressed data can hold"
        )
        .into());
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
