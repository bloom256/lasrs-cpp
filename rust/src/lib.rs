// SPDX-License-Identifier: MIT OR Apache-2.0

//! C ABI over the `las` crate.
//!
//! Conventions:
//! - Handles are opaque pointers created by `lasrs_*` constructors and
//!   released with the matching `lasrs_*_free`.
//! - Fallible functions return [`LasrsStatus`]; on failure the message is
//!   available from [`lasrs_last_error`] on the same thread.
//! - Strings and byte slices are passed as pointer + length and are only
//!   borrowed for the duration of the call, unless documented otherwise.
//! - Every pointer argument must be valid; null is never accepted except
//!   where a length of zero makes the pointer unused.

#![deny(unsafe_op_in_unsafe_fn)]
#![allow(clippy::missing_safety_doc)]

mod builder;
mod copc;
mod error;
mod header;
mod point_data;
mod reader;
mod stream;
mod types;
mod values;
mod writer;

pub use error::{LasrsStatus, lasrs_last_error};

/// Bumped on every incompatible change of the C ABI.
pub const LASRS_ABI_VERSION: u32 = 1;

#[unsafe(no_mangle)]
pub extern "C" fn lasrs_abi_version() -> u32 {
    LASRS_ABI_VERSION
}

pub(crate) fn into_handle<T>(value: T) -> *mut T {
    Box::into_raw(Box::new(value))
}

/// # Safety
/// `handle` must be null or come from [`into_handle`] and not be used again.
pub(crate) unsafe fn free_handle<T>(handle: *mut T) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
