// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{LasrsStatus, guard},
    types::{LasrsBounds, LasrsFormat, LasrsTransform, LasrsTransforms, borrow_slice_mut},
};
use las::{
    Bounds, Transform,
    point::{Classification, Format},
};
use std::fmt::Display;

/// Copies as much of the `Display` text as fits into `buf` and returns its
/// full length, so a caller can size the buffer with a first call.
///
/// # Safety
/// If `capacity > 0`, `buf` must point to `capacity` writable bytes.
unsafe fn copy_display(value: impl Display, buf: *mut u8, capacity: usize) -> usize {
    let text = value.to_string();
    let n = text.len().min(capacity);
    unsafe { borrow_slice_mut(buf, n) }.copy_from_slice(&text.as_bytes()[..n]);
    text.len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_format_display(
    format: LasrsFormat,
    buf: *mut u8,
    capacity: usize,
) -> usize {
    unsafe { copy_display(Format::from(format), buf, capacity) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_transform_display(
    transform: LasrsTransform,
    buf: *mut u8,
    capacity: usize,
) -> usize {
    unsafe { copy_display(Transform::from(transform), buf, capacity) }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_format_new(n: u8, out: *mut LasrsFormat) -> LasrsStatus {
    guard(|| {
        let format = Format::new(n)?;
        unsafe { out.write(format.into()) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_format_len(format: LasrsFormat) -> u16 {
    Format::from(format).len()
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_format_to_u8(format: LasrsFormat, out: *mut u8) -> LasrsStatus {
    guard(|| {
        let n = Format::from(format).to_u8()?;
        unsafe { out.write(n) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_classification_new(n: u8) -> LasrsStatus {
    guard(|| {
        let _ = Classification::new(n)?;
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_transform_inverse(
    transform: LasrsTransform,
    n: f64,
    out: *mut i32,
) -> LasrsStatus {
    guard(|| {
        let value = Transform::from(transform).inverse(n)?;
        unsafe { out.write(value) };
        Ok(())
    })
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn lasrs_bounds_adapt(
    bounds: LasrsBounds,
    transforms: LasrsTransforms,
    out: *mut LasrsBounds,
) -> LasrsStatus {
    guard(|| {
        let adapted = Bounds::from(bounds).adapt(&transforms.into())?;
        unsafe { out.write(adapted.into()) };
        Ok(())
    })
}
