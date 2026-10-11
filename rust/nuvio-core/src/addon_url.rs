//! Add-on URL normalization, request construction and ID encoding.
//!
//! Preserve the existing byte-oriented C behavior, including capacity checks
//! before removing a manifest suffix. Provider strings need not be UTF-8.
//! These functions allocate nothing, perform no I/O and retain no pointers.

use std::ffi::{c_char, CStr};

const BASE_CAPACITY: usize = 2048;
const REQUEST_CAPACITY: usize = BASE_CAPACITY + 1024;
const MANIFEST_SUFFIX: &[u8] = b"/manifest.json";
const HEX: &[u8; 16] = b"0123456789ABCDEF";

// All FFI callers must supply readable NUL-terminated input and writable output
// of the stated capacity. Input/output must not overlap. Null inputs mean empty
// strings; null/zero-capacity outputs are handled without dereferencing them.
unsafe fn input_bytes<'a>(input: *const c_char) -> &'a [u8] {
    if input.is_null() {
        &[]
    } else {
        unsafe { CStr::from_ptr(input) }.to_bytes()
    }
}

unsafe fn output_bytes<'a>(output: *mut c_char, capacity: usize) -> Option<&'a mut [u8]> {
    if output.is_null() || capacity == 0 {
        None
    } else {
        Some(unsafe { std::slice::from_raw_parts_mut(output.cast::<u8>(), capacity) })
    }
}

fn is_c_whitespace(byte: u8) -> bool {
    // C-locale isspace includes vertical tab; Unicode trimming would change IDs.
    matches!(byte, b' ' | b'\t' | b'\n' | b'\x0b' | b'\x0c' | b'\r')
}

fn trim_slashes(bytes: &[u8]) -> &[u8] {
    let end = bytes
        .iter()
        .rposition(|&byte| byte != b'/')
        .map_or(0, |i| i + 1);
    &bytes[..end]
}

fn split_query(bytes: &[u8]) -> (&[u8], &[u8]) {
    let query = bytes
        .iter()
        .position(|&byte| byte == b'?')
        .unwrap_or(bytes.len());
    bytes.split_at(query)
}

fn normalize_base(input: &[u8], output: &mut [u8]) {
    output[0] = 0;
    let start = input
        .iter()
        .position(|&byte| !is_c_whitespace(byte))
        .unwrap_or(input.len());
    let input = &input[start..];
    let end = input
        .iter()
        .rposition(|&byte| !is_c_whitespace(byte))
        .map_or(0, |i| i + 1);
    let (path, query) = split_query(&input[..end]);
    let (prefix, path) = if path
        .get(..10)
        .is_some_and(|scheme| scheme.eq_ignore_ascii_case(b"stremio://"))
    {
        (b"https://".as_slice(), &path[10..])
    } else {
        (b"".as_slice(), path)
    };
    let length = prefix.len() + path.len();
    // Preserve the C rule: reject an oversized original path even if removing
    // slashes or /manifest.json would later make the normalized value fit.
    if length >= output.len() {
        return;
    }
    output[..prefix.len()].copy_from_slice(prefix);
    output[prefix.len()..length].copy_from_slice(path);
    let mut length = trim_slashes(&output[..length]).len();
    if length > MANIFEST_SUFFIX.len()
        && output[length - MANIFEST_SUFFIX.len()..length].eq_ignore_ascii_case(MANIFEST_SUFFIX)
    {
        length -= MANIFEST_SUFFIX.len();
    }
    length = trim_slashes(&output[..length]).len();
    if query.len() > 1 {
        if query.len() >= output.len() - length {
            output[0] = 0;
            return;
        }
        output[length..length + query.len()].copy_from_slice(query);
        length += query.len();
    }
    output[length] = 0;
}

fn encode_id(input: &[u8], output: &mut [u8]) -> i32 {
    let mut length = 0;
    for &byte in input {
        let raw = byte.is_ascii_alphanumeric() || b"-._~:@!$&'()*+,;=".contains(&byte);
        let width = if raw { 1 } else { 3 };
        if width >= output.len() - length {
            output[0] = 0;
            return 0;
        }
        if raw {
            output[length] = byte;
        } else {
            output[length] = b'%';
            output[length + 1] = HEX[(byte >> 4) as usize];
            output[length + 2] = HEX[(byte & 15) as usize];
        }
        length += width;
    }
    output[length] = 0;
    1
}

fn request_length(length: usize) -> i32 {
    // Real callers use small bounded buffers. Reject sizes that cannot be
    // represented by the C return type rather than overflowing an int.
    i32::try_from(length).unwrap_or(-1)
}

/// Whether an installed URL fits the shared base buffer (including its NUL).
#[no_mangle]
pub extern "C" fn nuvio_rust_addon_url_fits(length: usize) -> i32 {
    i32::from(length < BASE_CAPACITY)
}

/// Whether a formatted request fits the caller's buffer (including its NUL).
#[no_mangle]
pub extern "C" fn nuvio_rust_addon_request_fits(length: i32, capacity: usize) -> i32 {
    i32::from(length >= 0 && (length as usize) < capacity)
}

/// Normalize an installed URL; insufficient capacity produces an empty string.
///
/// # Safety
/// Input is NUL-terminated bytes or null. Output is writable for `capacity`
/// bytes or null. The buffers must not overlap; neither pointer is retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_addon_base(
    input: *const c_char,
    output: *mut c_char,
    capacity: usize,
) {
    if let Some(output) = unsafe { output_bytes(output, capacity) } {
        normalize_base(unsafe { input_bytes(input) }, output);
    }
}

/// Encode an ID byte by byte, preserving the existing path-character allowlist.
/// Returns 1 on success, 0 with an empty output when it does not fit.
///
/// # Safety
/// Input is NUL-terminated bytes or null. Output is writable for `capacity`
/// bytes or null. The buffers must not overlap; neither pointer is retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_addon_id(
    output: *mut c_char,
    capacity: usize,
    input: *const c_char,
) -> i32 {
    match unsafe { output_bytes(output, capacity) } {
        Some(output) => encode_id(unsafe { input_bytes(input) }, output),
        None => 0,
    }
}

/// Append a C-formatted path before the base query and reject truncated output.
/// Returns the required byte length, or -1 for invalid output/format failure.
/// Oversized formatted paths preserve the C early-return length (no query).
///
/// # Safety
/// Base/path are readable NUL-terminated bytes or null; path must be non-null
/// for a successful format. `formatted_length` is the C vsnprintf result;
/// path is ignored on format failure or when that result is >= 3072.
/// Output is writable for `capacity` bytes or null. Inputs/output must not
/// overlap. No pointers are retained and no allocations cross the ABI.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_addon_request(
    output: *mut c_char,
    capacity: usize,
    base: *const c_char,
    path: *const c_char,
    formatted_length: i32,
) -> i32 {
    let (base, query) = split_query(unsafe { input_bytes(base) });
    let base = trim_slashes(base);
    let output = unsafe { output_bytes(output, capacity) };
    if formatted_length < 0 || formatted_length as usize >= REQUEST_CAPACITY {
        if let Some(output) = output {
            output[0] = 0;
        }
        return if formatted_length < 0 {
            -1
        } else {
            request_length(base.len() + formatted_length as usize)
        };
    }
    let Some(output) = output else { return -1 };
    output[0] = 0;
    if path.is_null() {
        return -1;
    }
    // C formatting may produce an embedded NUL. snprintf joins only the
    // visible part of that path, so do not substitute formatted_length here.
    let path = unsafe { input_bytes(path) };
    let length = base.len() + path.len() + query.len();
    let result = request_length(length);
    if result < 0 || length >= output.len() {
        return result;
    }
    output[..base.len()].copy_from_slice(base);
    output[base.len()..base.len() + path.len()].copy_from_slice(path);
    output[base.len() + path.len()..length].copy_from_slice(query);
    output[length] = 0;
    result
}
