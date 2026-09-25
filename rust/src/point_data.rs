// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{LasrsStatus, guard},
    free_handle, into_handle,
    types::{
        LasrsBytes, LasrsColor, LasrsFormat, LasrsPoint, LasrsTransforms, borrow_slice_mut,
        points_from_ffi,
    },
};
use las::{PointData, PointDataBuilder};

pub struct LasrsPointData(pub(crate) PointData);

/// # Safety
/// `points` must be a valid handle for `'a`.
unsafe fn data<'a>(points: *const LasrsPointData) -> &'a PointData {
    unsafe { &(*points).0 }
}

fn builder(format: LasrsFormat, transforms: LasrsTransforms) -> PointDataBuilder {
    PointDataBuilder::new()
        .with_format(format.into())
        .with_transforms(transforms.into())
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_point_data_build(
    format: LasrsFormat,
    transforms: LasrsTransforms,
) -> *mut LasrsPointData {
    into_handle(LasrsPointData(builder(format, transforms).build()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_build_from_bytes(
    format: LasrsFormat,
    transforms: LasrsTransforms,
    bytes: LasrsBytes,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let bytes = unsafe { bytes.as_slice() }.to_vec();
        let points = builder(format, transforms).build_from_bytes(bytes)?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_build_from_points(
    format: LasrsFormat,
    transforms: LasrsTransforms,
    points: *const LasrsPoint,
    n: usize,
    extra_bytes: *const u8,
    out: *mut *mut LasrsPointData,
) -> LasrsStatus {
    guard(|| {
        let points = unsafe { points_from_ffi(points, n, extra_bytes) }?;
        let points = builder(format, transforms).build_from_points(points)?;
        unsafe { out.write(into_handle(LasrsPointData(points))) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_clone(
    points: *const LasrsPointData,
) -> *mut LasrsPointData {
    into_handle(LasrsPointData(unsafe { data(points) }.clone()))
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_free(points: *mut LasrsPointData) {
    unsafe { free_handle(points) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_len(points: *const LasrsPointData) -> usize {
    unsafe { data(points) }.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_format(points: *const LasrsPointData) -> LasrsFormat {
    (*unsafe { data(points) }.format()).into()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_transforms(
    points: *const LasrsPointData,
) -> LasrsTransforms {
    (*unsafe { data(points) }.transforms()).into()
}

/// Borrowed, valid until the point data is modified or freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_raw_bytes(points: *const LasrsPointData) -> LasrsBytes {
    LasrsBytes::new(unsafe { data(points) }.raw_bytes())
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_record_len(points: *const LasrsPointData) -> usize {
    unsafe { data(points) }.record_len()
}

/// Resizes to `n` points; returns the writable byte slab
/// (`n * record_len` bytes), valid until the next modification.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_resize_for(
    points: *mut LasrsPointData,
    n: usize,
) -> *mut u8 {
    unsafe { (*points).0.resize_for(n) }.as_mut_ptr()
}

/// Decodes all points. `out` holds `len()` points and `extra_bytes_out`
/// holds `len() * format.extra_bytes` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_points(
    points: *const LasrsPointData,
    out: *mut LasrsPoint,
    extra_bytes_out: *mut u8,
) -> LasrsStatus {
    guard(|| {
        let points = unsafe { data(points) };
        let n = points.len();
        let extra_len = usize::from(points.format().extra_bytes);
        let out = unsafe { borrow_slice_mut(out, n) };
        let extra_out = unsafe { borrow_slice_mut(extra_bytes_out, n * extra_len) };
        for (i, (point, slot)) in points.points().zip(out.iter_mut()).enumerate() {
            let point = point?;
            *slot = LasrsPoint::from(&point);
            extra_out[i * extra_len..][..point.extra_bytes.len()]
                .copy_from_slice(&point.extra_bytes);
        }
        Ok(())
    })
}

/// # Safety
/// `out` must hold `n` elements.
unsafe fn fill_n<T>(values: impl Iterator<Item = T>, n: usize, out: *mut T) {
    let out = unsafe { borrow_slice_mut(out, n) };
    for (slot, value) in out.iter_mut().zip(values) {
        *slot = value;
    }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_x_raw(points: *const LasrsPointData, out: *mut i32) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.x_raw(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_y_raw(points: *const LasrsPointData, out: *mut i32) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.y_raw(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_z_raw(points: *const LasrsPointData, out: *mut i32) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.z_raw(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_x(points: *const LasrsPointData, out: *mut f64) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.x(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_y(points: *const LasrsPointData, out: *mut f64) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.y(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_z(points: *const LasrsPointData, out: *mut f64) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.z(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_intensity(points: *const LasrsPointData, out: *mut u16) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.intensity(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_classification(
    points: *const LasrsPointData,
    out: *mut u8,
) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.classification(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_return_number(
    points: *const LasrsPointData,
    out: *mut u8,
) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.return_number(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_number_of_returns(
    points: *const LasrsPointData,
    out: *mut u8,
) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.number_of_returns(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_scan_angle_degrees(
    points: *const LasrsPointData,
    out: *mut f32,
) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.scan_angle_degrees(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_user_data(points: *const LasrsPointData, out: *mut u8) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.user_data(), points.len(), out) }
}

/// Writes `len()` values to `out`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_point_source_id(
    points: *const LasrsPointData,
    out: *mut u16,
) {
    let points = unsafe { data(points) };
    unsafe { fill_n(points.point_source_id(), points.len(), out) }
}

/// Returns false (and writes nothing) if the format has no GPS time.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_gps_time(
    points: *const LasrsPointData,
    out: *mut f64,
) -> bool {
    let points = unsafe { data(points) };
    points
        .gps_time()
        .map(|values| unsafe { fill_n(values, points.len(), out) })
        .is_some()
}

/// Returns false (and writes nothing) if the format has no color.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_rgb(
    points: *const LasrsPointData,
    out: *mut LasrsColor,
) -> bool {
    let points = unsafe { data(points) };
    points
        .rgb()
        .map(|values| {
            let colors = values.map(|(red, green, blue)| LasrsColor { red, green, blue });
            unsafe { fill_n(colors, points.len(), out) }
        })
        .is_some()
}

/// Returns false (and writes nothing) if the format has no NIR.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_point_data_nir(
    points: *const LasrsPointData,
    out: *mut u16,
) -> bool {
    let points = unsafe { data(points) };
    points
        .nir()
        .map(|values| unsafe { fill_n(values, points.len(), out) })
        .is_some()
}
