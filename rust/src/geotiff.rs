// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{LasrsStatus, guard},
    free_handle,
    header::{LasrsHeader, header_ref},
    into_handle,
    types::LasrsStr,
};
use las::crs::{GeoTiffCrs, GeoTiffData};
use std::ptr;

pub struct LasrsGeoTiffCrs(GeoTiffCrs);

#[repr(C)]
#[derive(Clone, Copy)]
pub enum LasrsGeoTiffDataKind {
    U16 = 0,
    String = 1,
    Doubles = 2,
}

/// Borrowed view of a `GeoTiffKeyEntry`; only the field matching `kind` is
/// set, the others are zero.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsGeoTiffKeyEntry {
    pub id: u16,
    pub kind: LasrsGeoTiffDataKind,
    pub u16_value: u16,
    pub string: LasrsStr,
    pub doubles: *const f64,
    pub doubles_len: usize,
}

/// Writes null to `out` if the header has no GeoTIFF CRS.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_get_geotiff_crs(
    header: *const LasrsHeader,
    out: *mut *mut LasrsGeoTiffCrs,
) -> LasrsStatus {
    guard(|| {
        let crs = unsafe { header_ref(header) }.get_geotiff_crs()?;
        let handle = crs.map_or(ptr::null_mut(), |crs| into_handle(LasrsGeoTiffCrs(crs)));
        unsafe { out.write(handle) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_geotiff_crs_free(crs: *mut LasrsGeoTiffCrs) {
    unsafe { free_handle(crs) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_geotiff_crs_entries_len(crs: *const LasrsGeoTiffCrs) -> usize {
    unsafe { &(*crs).0 }.entries.len()
}

/// Borrowed view, valid while the CRS handle is alive.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_geotiff_crs_entry(
    crs: *const LasrsGeoTiffCrs,
    index: usize,
) -> LasrsGeoTiffKeyEntry {
    let entry = &unsafe { &(*crs).0 }.entries[index];
    let mut view = LasrsGeoTiffKeyEntry {
        id: entry.id,
        kind: LasrsGeoTiffDataKind::U16,
        u16_value: 0,
        string: LasrsStr::new(""),
        doubles: ptr::null(),
        doubles_len: 0,
    };
    match &entry.data {
        GeoTiffData::U16(value) => view.u16_value = *value,
        GeoTiffData::String(string) => {
            view.kind = LasrsGeoTiffDataKind::String;
            view.string = LasrsStr::new(string);
        }
        GeoTiffData::Doubles(doubles) => {
            view.kind = LasrsGeoTiffDataKind::Doubles;
            view.doubles = doubles.as_ptr();
            view.doubles_len = doubles.len();
        }
    }
    view
}
