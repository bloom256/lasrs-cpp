// SPDX-License-Identifier: MIT OR Apache-2.0

use crate::types::LasrsStr;
use std::{
    any::Any,
    cell::RefCell,
    panic::{AssertUnwindSafe, catch_unwind},
};

#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LasrsStatus {
    Ok = 0,
    Error = 1,
    Panic = 2,
}

pub(crate) type BoxError = Box<dyn std::error::Error + Send + Sync>;

thread_local! {
    static LAST_ERROR: RefCell<String> = const { RefCell::new(String::new()) };
}

/// Message of the last failed call on this thread. Valid until the next
/// failing call on the same thread.
#[unsafe(no_mangle)]
pub extern "C" fn lasrs_last_error() -> LasrsStr {
    LAST_ERROR.with_borrow(|message| LasrsStr::new(message))
}

fn set_last_error(message: String) {
    LAST_ERROR.with_borrow_mut(|last| *last = message);
}

fn panic_message(payload: &(dyn Any + Send)) -> String {
    let detail = payload
        .downcast_ref::<&str>()
        .map(|s| s.to_string())
        .or_else(|| payload.downcast_ref::<String>().cloned())
        .unwrap_or_else(|| "unknown panic".to_string());
    format!("panic in lasrs: {detail}")
}

/// Runs `f`, converting errors and panics into a status code.
pub(crate) fn guard(f: impl FnOnce() -> Result<(), BoxError>) -> LasrsStatus {
    match catch_unwind(AssertUnwindSafe(f)) {
        Ok(Ok(())) => LasrsStatus::Ok,
        Ok(Err(error)) => {
            set_last_error(error.to_string());
            LasrsStatus::Error
        }
        Err(payload) => {
            set_last_error(panic_message(payload.as_ref()));
            LasrsStatus::Panic
        }
    }
}
