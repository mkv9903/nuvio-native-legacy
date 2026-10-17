//! Stateless projections for native text fields; domain text stays unbounded.
use crate::search_text;
use std::ffi::{c_char, CStr};

unsafe fn bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        unsafe { CStr::from_ptr(text) }.to_bytes()
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_text_normalize(
    text: *const c_char,
    output: *mut c_char,
    capacity: usize,
) {
    if output.is_null() || capacity == 0 {
        return;
    }
    let normalized = search_text::local_key(&search_text::decode_input(unsafe { bytes(text) }));
    let prefix = search_text::prefix(&normalized, capacity - 1);
    unsafe {
        std::ptr::copy_nonoverlapping(prefix.as_ptr(), output.cast(), prefix.len());
        *output.add(prefix.len()) = 0;
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_text_count(text: *const c_char) -> i32 {
    search_text::character_count(unsafe { bytes(text) }).min(i32::MAX as usize) as i32
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_text_delete_last(text: *mut c_char, length: usize) -> usize {
    if text.is_null() {
        return 0;
    }
    let index = search_text::last_character_start(unsafe {
        std::slice::from_raw_parts(text.cast(), length)
    });
    unsafe {
        *text.add(index) = 0;
    }
    index
}
