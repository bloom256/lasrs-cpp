// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::{
    error::{LasrsStatus, guard},
    types::{LasrsBounds, LasrsFormat, LasrsTransform, LasrsTransforms},
};
use las::{Bounds, Transform, point::Format};

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
