// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{BoxError, LasrsStatus, guard},
    free_handle,
    header::{LasrsDate, LasrsHeader, header_ref},
    into_handle,
    types::{
        LasrsBytes, LasrsFormat, LasrsStr, LasrsTransforms, LasrsVersion, LasrsVlr,
        gps_time_type_from_u8, gps_time_type_to_u8, vlrs_from_ffi,
    },
};
use chrono::{Datelike, NaiveDate};
use las::Builder;
use uuid::Uuid;

pub struct LasrsBuilder(Builder);

/// All scalar and byte fields of a [`Builder`]; VLRs are passed separately.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsBuilderFields {
    pub has_date: bool,
    pub date: LasrsDate,
    pub file_source_id: u16,
    pub generating_software: LasrsStr,
    pub gps_time_type: u8,
    pub guid: [u8; 16],
    pub has_synthetic_return_numbers: bool,
    pub has_wkt_crs: bool,
    pub padding: LasrsBytes,
    pub point_format: LasrsFormat,
    pub point_padding: LasrsBytes,
    pub system_identifier: LasrsStr,
    pub transforms: LasrsTransforms,
    pub version: LasrsVersion,
    pub vlr_padding: LasrsBytes,
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_builder_from_version(version: LasrsVersion) -> *mut LasrsBuilder {
    into_handle(LasrsBuilder(Builder::from(las::Version::from(version))))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_from_header(
    header: *const LasrsHeader,
) -> *mut LasrsBuilder {
    into_handle(LasrsBuilder(Builder::from(
        unsafe { header_ref(header) }.clone(),
    )))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_free(builder: *mut LasrsBuilder) {
    unsafe { free_handle(builder) }
}

/// Borrowed view, valid until the builder is freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_fields(builder: *const LasrsBuilder) -> LasrsBuilderFields {
    let b = unsafe { &(*builder).0 };
    LasrsBuilderFields {
        has_date: b.date.is_some(),
        date: b
            .date
            .map(|d| LasrsDate {
                year: d.year(),
                month: d.month(),
                day: d.day(),
            })
            .unwrap_or_default(),
        file_source_id: b.file_source_id,
        generating_software: LasrsStr::new(&b.generating_software),
        gps_time_type: gps_time_type_to_u8(b.gps_time_type),
        guid: *b.guid.as_bytes(),
        has_synthetic_return_numbers: b.has_synthetic_return_numbers,
        has_wkt_crs: b.has_wkt_crs,
        padding: LasrsBytes::new(&b.padding),
        point_format: b.point_format.into(),
        point_padding: LasrsBytes::new(&b.point_padding),
        system_identifier: LasrsStr::new(&b.system_identifier),
        transforms: b.transforms.into(),
        version: b.version.into(),
        vlr_padding: LasrsBytes::new(&b.vlr_padding),
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_vlrs_len(builder: *const LasrsBuilder) -> usize {
    unsafe { &(*builder).0 }.vlrs.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_vlr(builder: *const LasrsBuilder, index: usize) -> LasrsVlr {
    (&unsafe { &(*builder).0 }.vlrs[index]).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_evlrs_len(builder: *const LasrsBuilder) -> usize {
    unsafe { &(*builder).0 }.evlrs.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_evlr(
    builder: *const LasrsBuilder,
    index: usize,
) -> LasrsVlr {
    (&unsafe { &(*builder).0 }.evlrs[index]).into()
}

/// Applies `fields` and VLRs on top of `base` (or a default builder when
/// `base` is null), so fields not visible to C are preserved.
///
/// # Safety
/// All pointers must be valid; `vlrs` and `evlrs` hold `n_vlrs` / `n_evlrs`.
unsafe fn builder_from_ffi(
    base: *const LasrsBuilder,
    fields: *const LasrsBuilderFields,
    vlrs: *const LasrsVlr,
    n_vlrs: usize,
    evlrs: *const LasrsVlr,
    n_evlrs: usize,
) -> Result<Builder, BoxError> {
    let f = unsafe { *fields };
    let date = if f.has_date {
        Some(
            NaiveDate::from_ymd_opt(f.date.year, f.date.month, f.date.day).ok_or_else(|| {
                format!(
                    "invalid date: {}-{}-{}",
                    f.date.year, f.date.month, f.date.day
                )
            })?,
        )
    } else {
        None
    };
    let mut builder = if base.is_null() {
        Builder::default()
    } else {
        unsafe { &(*base).0 }.clone()
    };
    builder.version = f.version.into();
    builder.date = date;
    builder.file_source_id = f.file_source_id;
    builder.generating_software = unsafe { f.generating_software.to_str() }?.to_string();
    builder.gps_time_type = gps_time_type_from_u8(f.gps_time_type);
    builder.guid = Uuid::from_bytes(f.guid);
    builder.has_synthetic_return_numbers = f.has_synthetic_return_numbers;
    builder.has_wkt_crs = f.has_wkt_crs;
    builder.padding = unsafe { f.padding.as_slice() }.to_vec();
    builder.point_format = f.point_format.into();
    builder.point_padding = unsafe { f.point_padding.as_slice() }.to_vec();
    builder.system_identifier = unsafe { f.system_identifier.to_str() }?.to_string();
    builder.transforms = f.transforms.into();
    builder.vlr_padding = unsafe { f.vlr_padding.as_slice() }.to_vec();
    builder.vlrs = unsafe { vlrs_from_ffi(vlrs, n_vlrs) }?;
    builder.evlrs = unsafe { vlrs_from_ffi(evlrs, n_evlrs) }?;
    Ok(builder)
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_into_header(
    base: *const LasrsBuilder,
    fields: *const LasrsBuilderFields,
    vlrs: *const LasrsVlr,
    n_vlrs: usize,
    evlrs: *const LasrsVlr,
    n_evlrs: usize,
    out: *mut *mut LasrsHeader,
) -> LasrsStatus {
    guard(|| {
        let builder = unsafe { builder_from_ffi(base, fields, vlrs, n_vlrs, evlrs, n_evlrs) }?;
        let header = builder.into_header()?;
        unsafe { out.write(LasrsHeader::new_handle(header)) };
        Ok(())
    })
}

/// Writes `has_version = false` if no version supports the configuration.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_builder_minimum_supported_version(
    base: *const LasrsBuilder,
    fields: *const LasrsBuilderFields,
    vlrs: *const LasrsVlr,
    n_vlrs: usize,
    evlrs: *const LasrsVlr,
    n_evlrs: usize,
    has_version: *mut bool,
    out: *mut LasrsVersion,
) -> LasrsStatus {
    guard(|| {
        let builder = unsafe { builder_from_ffi(base, fields, vlrs, n_vlrs, evlrs, n_evlrs) }?;
        let version = builder.minimum_supported_version();
        unsafe {
            has_version.write(version.is_some());
            if let Some(v) = version {
                out.write(v.into());
            }
        }
        Ok(())
    })
}
