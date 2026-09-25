// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{LasrsStatus, guard},
    free_handle, into_handle,
    point_data::LasrsPointData,
    types::{
        LasrsBounds, LasrsBytes, LasrsFormat, LasrsPoint, LasrsStr, LasrsTransforms, LasrsVersion,
        LasrsVlr, borrow_slice, gps_time_type_to_u8,
    },
};
use chrono::Datelike;
use las::Header;

/// Opaque handle; points at a `las::Header`.
pub struct LasrsHeader {
    _private: [u8; 0],
}

impl LasrsHeader {
    pub(crate) fn new_handle(header: Header) -> *mut LasrsHeader {
        into_handle(header).cast()
    }

    pub(crate) fn borrow(header: &Header) -> *const LasrsHeader {
        (header as *const Header).cast()
    }
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct LasrsDate {
    pub year: i32,
    pub month: u32,
    pub day: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct LasrsCopcInfoVlr {
    pub center_x: f64,
    pub center_y: f64,
    pub center_z: f64,
    pub halfsize: f64,
    pub spacing: f64,
    pub gpstime_minimum: f64,
    pub gpstime_maximum: f64,
}

/// # Safety
/// `header` must be a valid handle for `'a`.
pub(crate) unsafe fn header_ref<'a>(header: *const LasrsHeader) -> &'a Header {
    unsafe { &*header.cast::<Header>() }
}

/// # Safety
/// `header` must be a valid handle for `'a`.
unsafe fn header_mut<'a>(header: *mut LasrsHeader) -> &'a mut Header {
    unsafe { &mut *header.cast::<Header>() }
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_header_default() -> *mut LasrsHeader {
    LasrsHeader::new_handle(Header::default())
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_header_from_version(version: LasrsVersion) -> *mut LasrsHeader {
    LasrsHeader::new_handle(Header::from(las::Version::from(version)))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_clone(header: *const LasrsHeader) -> *mut LasrsHeader {
    LasrsHeader::new_handle(unsafe { header_ref(header) }.clone())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_free(header: *mut LasrsHeader) {
    unsafe { free_handle(header.cast::<Header>()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_eq(a: *const LasrsHeader, b: *const LasrsHeader) -> bool {
    unsafe { header_ref(a) == header_ref(b) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_file_source_id(header: *const LasrsHeader) -> u16 {
    unsafe { header_ref(header) }.file_source_id()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_gps_time_type(header: *const LasrsHeader) -> u8 {
    gps_time_type_to_u8(unsafe { header_ref(header) }.gps_time_type())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_has_synthetic_return_numbers(
    header: *const LasrsHeader,
) -> bool {
    unsafe { header_ref(header) }.has_synthetic_return_numbers()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_has_wkt_crs(header: *const LasrsHeader) -> bool {
    unsafe { header_ref(header) }.has_wkt_crs()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_guid(header: *const LasrsHeader, out: *mut [u8; 16]) {
    unsafe { out.write(*header_ref(header).guid().as_bytes()) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_version(header: *const LasrsHeader) -> LasrsVersion {
    unsafe { header_ref(header) }.version().into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_system_identifier(header: *const LasrsHeader) -> LasrsStr {
    LasrsStr::new(unsafe { header_ref(header) }.system_identifier())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_generating_software(header: *const LasrsHeader) -> LasrsStr {
    LasrsStr::new(unsafe { header_ref(header) }.generating_software())
}

/// Returns false if the header has no date.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_date(
    header: *const LasrsHeader,
    out: *mut LasrsDate,
) -> bool {
    match unsafe { header_ref(header) }.date() {
        Some(date) => {
            unsafe {
                out.write(LasrsDate {
                    year: date.year(),
                    month: date.month(),
                    day: date.day(),
                })
            };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_padding(header: *const LasrsHeader) -> LasrsBytes {
    LasrsBytes::new(unsafe { header_ref(header) }.padding())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_point_format(header: *const LasrsHeader) -> LasrsFormat {
    (*unsafe { header_ref(header) }.point_format()).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_transforms(header: *const LasrsHeader) -> LasrsTransforms {
    (*unsafe { header_ref(header) }.transforms()).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_bounds(header: *const LasrsHeader) -> LasrsBounds {
    unsafe { header_ref(header) }.bounds().into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_number_of_points(header: *const LasrsHeader) -> u64 {
    unsafe { header_ref(header) }.number_of_points()
}

/// Returns false if `n` is not a valid return number for this header.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_number_of_points_by_return(
    header: *const LasrsHeader,
    n: u8,
    out: *mut u64,
) -> bool {
    match unsafe { header_ref(header) }.number_of_points_by_return(n) {
        Some(count) => {
            unsafe { out.write(count) };
            true
        }
        None => false,
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_vlr_padding(header: *const LasrsHeader) -> LasrsBytes {
    LasrsBytes::new(unsafe { header_ref(header) }.vlr_padding())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_point_padding(header: *const LasrsHeader) -> LasrsBytes {
    LasrsBytes::new(unsafe { header_ref(header) }.point_padding())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_vlrs_len(header: *const LasrsHeader) -> usize {
    unsafe { header_ref(header) }.vlrs().len()
}

/// Borrowed view, valid until the header is modified or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_vlr(header: *const LasrsHeader, index: usize) -> LasrsVlr {
    (&unsafe { header_ref(header) }.vlrs()[index]).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_evlrs_len(header: *const LasrsHeader) -> usize {
    unsafe { header_ref(header) }.evlrs().len()
}

/// Borrowed view, valid until the header is modified or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_evlr(header: *const LasrsHeader, index: usize) -> LasrsVlr {
    (&unsafe { header_ref(header) }.evlrs()[index]).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_has_crs_vlrs(header: *const LasrsHeader) -> bool {
    unsafe { header_ref(header) }.has_crs_vlrs()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_clear(header: *mut LasrsHeader) {
    unsafe { header_mut(header) }.clear()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_add_point(
    header: *mut LasrsHeader,
    point: *const LasrsPoint,
    extra_bytes: *const u8,
) -> LasrsStatus {
    guard(|| {
        let point = unsafe { *point };
        let extra = unsafe { borrow_slice(extra_bytes, point.extra_bytes_len) };
        unsafe { header_mut(header) }.add_point(&point.to_point(extra)?);
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_add_point_data(
    header: *mut LasrsHeader,
    points: *const LasrsPointData,
) {
    unsafe { header_mut(header).add_point_data(&(*points).0) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_remove_crs_vlrs(header: *mut LasrsHeader) {
    unsafe { header_mut(header) }.remove_crs_vlrs()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_set_wkt_crs(
    header: *mut LasrsHeader,
    wkt_crs_bytes: LasrsBytes,
) -> LasrsStatus {
    guard(|| {
        let bytes = unsafe { wkt_crs_bytes.as_slice() }.to_vec();
        unsafe { header_mut(header) }.set_wkt_crs(bytes)?;
        Ok(())
    })
}

/// Returns false if the header has no WKT CRS. The bytes are borrowed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_get_wkt_crs_bytes(
    header: *const LasrsHeader,
    out: *mut LasrsBytes,
) -> bool {
    match unsafe { header_ref(header) }.get_wkt_crs_bytes() {
        Some(bytes) => {
            unsafe { out.write(LasrsBytes::new(bytes)) };
            true
        }
        None => false,
    }
}

/// Returns false if the header has no COPC info VLR.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_header_copc_info_vlr(
    header: *const LasrsHeader,
    out: *mut LasrsCopcInfoVlr,
) -> bool {
    match unsafe { header_ref(header) }.copc_info_vlr() {
        Some(info) => {
            unsafe {
                out.write(LasrsCopcInfoVlr {
                    center_x: info.center_x,
                    center_y: info.center_y,
                    center_z: info.center_z,
                    halfsize: info.halfsize,
                    spacing: info.spacing,
                    gpstime_minimum: info.gpstime_minimum,
                    gpstime_maximum: info.gpstime_maximum,
                })
            };
            true
        }
        None => false,
    }
}
