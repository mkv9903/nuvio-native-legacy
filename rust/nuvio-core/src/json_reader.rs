//! Tolerant, bounded JSON scanning behind the existing caller-owned C API.
//! This preserves the application reader, rather than validating full JSON.

use crate::utf8;
use std::ffi::{c_char, c_int, c_long, CStr};
use std::ptr;

const KEEP_LINES: u32 = 1;
const ROOT_ONLY: u32 = 2;

// GNU libc's native layout on the two current targets: Linux and webOS ARM.
// Keep native time_t width, including the old TV runtime's date range.
#[repr(C)]
struct NativeTime {
    second: c_int,
    minute: c_int,
    hour: c_int,
    day: c_int,
    month: c_int,
    year: c_int,
    weekday: c_int,
    year_day: c_int,
    daylight: c_int,
    utc_offset: c_long,
    zone: *const c_char,
}

const _: () = {
    assert!(
        std::mem::size_of::<NativeTime>()
            == if cfg!(target_pointer_width = "64") {
                56
            } else {
                44
            }
    );
    assert!(
        std::mem::offset_of!(NativeTime, utc_offset)
            == if cfg!(target_pointer_width = "64") {
                40
            } else {
                36
            }
    );
    assert!(
        std::mem::offset_of!(NativeTime, zone)
            == if cfg!(target_pointer_width = "64") {
                48
            } else {
                40
            }
    );
};

extern "C" {
    fn strtod(input: *const c_char, end: *mut *mut c_char) -> f64;
    fn timegm(time: *mut NativeTime) -> c_long;
    // Same conversion routine used by the retained C reader on GNU targets.
    #[link_name = "__isoc99_sscanf"]
    fn scan_date(input: *const c_char, format: *const c_char, ...) -> c_int;
}

// Explicit ranges may contain embedded NUL and need no terminator at the end.
// SAFETY: non-null endpoints belong to the same readable allocation, in order.
unsafe fn input_range<'a>(start: *const c_char, end: *const c_char) -> Option<&'a [u8]> {
    if start.is_null() {
        return None;
    }
    if end.is_null() {
        return Some(unsafe { CStr::from_ptr(start) }.to_bytes());
    }
    if (end as usize) < start as usize {
        return None;
    }
    let length = unsafe { end.offset_from(start) } as usize;
    Some(unsafe { std::slice::from_raw_parts(start.cast(), length) })
}

unsafe fn output_range<'a>(output: *mut c_char, capacity: usize) -> Option<&'a mut [u8]> {
    if output.is_null() || capacity == 0 {
        return None;
    }
    Some(unsafe { std::slice::from_raw_parts_mut(output.cast(), capacity) })
}

fn skip_space(input: &[u8], mut index: usize) -> usize {
    while input
        .get(index)
        .is_some_and(|&byte| byte > 0 && byte <= b' ')
    {
        index += 1;
    }
    index
}

fn string_end(input: &[u8], mut index: usize) -> Option<usize> {
    index += 1;
    while let Some(&byte) = input.get(index) {
        match byte {
            0 => return None,
            b'"' => return Some(index),
            b'\\' => {
                if input.get(index + 1).is_none_or(|&next| next == 0) {
                    return None;
                }
                index += 1;
            }
            _ => {}
        }
        index += 1;
    }
    None
}

fn find_key(input: &[u8], mut index: usize, key: &[u8]) -> Option<usize> {
    while let Some(offset) = input[index..].iter().position(|&byte| byte == b'"') {
        index += offset;
        let close = string_end(input, index)?;
        let colon = skip_space(input, close + 1);
        if &input[index + 1..close] == key && input.get(colon) == Some(&b':') {
            return Some(colon + 1);
        }
        index = close + 1;
    }
    None
}

fn root_key(input: &[u8], key: &[u8]) -> Option<usize> {
    let mut index = input.iter().position(|&byte| byte == b'{')?;
    let mut depth = 0usize;
    while let Some(&byte) = input.get(index).filter(|&&byte| byte != 0) {
        match byte {
            b'"' => {
                let close = string_end(input, index)?;
                let colon = skip_space(input, close + 1);
                if depth == 1 && &input[index + 1..close] == key && input.get(colon) == Some(&b':')
                {
                    return Some(colon + 1);
                }
                index = close;
            }
            b'{' | b'[' => depth += 1,
            b'}' | b']' => {
                if depth <= 1 {
                    break;
                }
                depth -= 1;
            }
            _ => {}
        }
        index += 1;
    }
    None
}

fn hex4(input: &[u8]) -> Option<u32> {
    let mut value = 0;
    for &byte in input.get(..4)? {
        let digit = match byte {
            b'0'..=b'9' => byte - b'0',
            b'a'..=b'f' => byte - b'a' + 10,
            b'A'..=b'F' => byte - b'A' + 10,
            _ => return None,
        };
        value = value * 16 + u32::from(digit);
    }
    Some(value)
}

fn unicode_escape(input: &[u8]) -> (u32, usize) {
    let Some(mut codepoint) = hex4(input) else {
        // Malformed hex leaves those bytes for subsequent literal copying.
        return (u32::from(b' '), 0);
    };
    let mut consumed = 4;
    if (0xd800..=0xdbff).contains(&codepoint) && input.get(4..6) == Some(b"\\u") {
        if let Some(low) = hex4(&input[6..]).filter(|low| (0xdc00..=0xdfff).contains(low)) {
            codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
            consumed = 10;
        }
    }
    if (0xd800..=0xdfff).contains(&codepoint) {
        codepoint = u32::from(b' ');
    }
    (codepoint, consumed)
}

fn read_string(input: &[u8], mut index: usize, output: &mut [u8], keep_lines: bool) -> i32 {
    if input.get(index) != Some(&b'"') {
        return 0;
    }
    let Some(close) = string_end(input, index) else {
        return 0;
    };
    index += 1;
    let mut length = 0;
    while index < close && length + 1 < output.len() {
        if input[index] == b'\\' && close - index >= 2 {
            index += 1;
            if input[index] == b'u' {
                let (codepoint, consumed) = unicode_escape(&input[index + 1..close]);
                let mut encoded = [0; 4];
                let width = utf8::encode(codepoint, &mut encoded);
                if width >= output.len() - length {
                    // Approved correction: retain the prefix, never skip a
                    // character that will not fit and copy later text.
                    break;
                }
                output[length..length + width].copy_from_slice(&encoded[..width]);
                length += width;
                index += 1 + consumed;
                continue;
            }
            if keep_lines && input[index] == b'r' {
                index += 1;
                continue;
            }
            if matches!(input[index], b'n' | b'r' | b't') {
                output[length] = if keep_lines && input[index] == b'n' {
                    b'\n'
                } else {
                    b' '
                };
                length += 1;
                index += 1;
                continue;
            }
        }
        output[length] = input[index];
        length += 1;
        index += 1;
    }
    // Preserve the existing literal-byte policy, including malformed UTF-8.
    // If capacity interrupts a multibyte character, discard its partial tail.
    if index < close && length > 0 && output[length - 1] & 0x80 != 0 {
        let mut boundary = length;
        while boundary > 0 && output[boundary - 1] & 0xc0 == 0x80 {
            boundary -= 1;
        }
        if boundary > 0 {
            let lead = output[boundary - 1];
            let width = if lead >= 0xf0 {
                4
            } else if lead >= 0xe0 {
                3
            } else if lead >= 0xc0 {
                2
            } else {
                1
            };
            if length - (boundary - 1) < width {
                length = boundary - 1;
            }
        }
    }
    output[length] = 0;
    i32::from(length > 0)
}

fn container_end(input: &[u8], mut index: usize) -> Option<usize> {
    let open = input[index];
    let close = if open == b'[' { b']' } else { b'}' };
    let mut depth = 0usize;
    while let Some(&byte) = input.get(index).filter(|&&byte| byte != 0) {
        if byte == b'"' {
            index = string_end(input, index)?;
        } else if byte == open {
            depth += 1;
        } else if byte == close {
            depth -= 1;
            if depth == 0 {
                return Some(index + 1);
            }
        }
        index += 1;
    }
    None
}

fn first_element(input: &[u8], index: usize) -> Option<usize> {
    let index = skip_space(input, index);
    input
        .get(index)
        .filter(|&&byte| byte == b'{' || byte == b'"')
        .map(|_| index)
}

/// Find the end of a container; incomplete input returns its NUL position.
/// # Safety
/// Non-null `input` is readable and NUL-terminated. Returned pointers borrow it.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_end(input: *const c_char) -> *const c_char {
    if input.is_null() {
        return ptr::null();
    }
    let open = unsafe { *input.cast::<u8>() };
    if !matches!(open, b'{' | b'[') {
        return input;
    }
    // Walk only this container. Measuring the remaining document with strlen
    // for every element would make large account arrays quadratic again.
    let close = if open == b'[' { b']' } else { b'}' };
    let mut current = input;
    let mut depth = 0usize;
    let mut in_string = false;
    loop {
        let byte = unsafe { *current.cast::<u8>() };
        if byte == 0 {
            return current;
        }
        if in_string {
            if byte == b'\\' && unsafe { *current.add(1) } != 0 {
                current = unsafe { current.add(1) };
            } else if byte == b'"' {
                in_string = false;
            }
        } else if byte == b'"' {
            in_string = true;
        } else if byte == open {
            depth += 1;
        } else if byte == close {
            depth -= 1;
            if depth == 0 {
                return unsafe { current.add(1) };
            }
        }
        current = unsafe { current.add(1) };
    }
}

/// Check presence, including null and malformed values.
/// # Safety
/// Readable inputs follow the range and key contracts in `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_has(
    start: *const c_char,
    end: *const c_char,
    key: *const c_char,
) -> i32 {
    let (Some(input), Some(key)) = (unsafe { input_range(start, end) }, unsafe {
        input_range(key, ptr::null())
    }) else {
        return 0;
    };
    i32::from(find_key(input, 0, key).is_some())
}

/// Decode a named string, optionally keeping lines or checking root keys only.
/// # Safety
/// Inputs and disjoint writable output follow the contracts in `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_text(
    start: *const c_char,
    end: *const c_char,
    key: *const c_char,
    output: *mut c_char,
    capacity: usize,
    flags: u32,
) -> i32 {
    if start.is_null() || key.is_null() {
        return 0;
    }
    let Some(output) = (unsafe { output_range(output, capacity) }) else {
        return 0;
    };
    if flags & ROOT_ONLY != 0 {
        output[0] = 0;
    }
    let (Some(input), Some(key)) = (unsafe { input_range(start, end) }, unsafe {
        input_range(key, ptr::null())
    }) else {
        return 0;
    };
    let position = if flags & ROOT_ONLY != 0 {
        root_key(input, key)
    } else {
        find_key(input, 0, key)
    };
    position.map_or(0, |index| {
        read_string(
            input,
            skip_space(input, index),
            output,
            flags & KEEP_LINES != 0,
        )
    })
}

/// Decode a standalone NUL-terminated JSON string.
/// # Safety
/// Input and disjoint output follow the contracts in `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_string(
    input: *const c_char,
    output: *mut c_char,
    capacity: usize,
) -> i32 {
    let (Some(input), Some(output)) = (unsafe { input_range(input, ptr::null()) }, unsafe {
        output_range(output, capacity)
    }) else {
        return 0;
    };
    read_string(input, 0, output, false)
}

/// Read a finite number, accepting quoted values and nested occurrences.
/// # Safety
/// Readable inputs follow the range and key contracts in `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_number(
    start: *const c_char,
    end: *const c_char,
    key: *const c_char,
    fallback: f64,
) -> f64 {
    let (Some(input), Some(key)) = (unsafe { input_range(start, end) }, unsafe {
        input_range(key, ptr::null())
    }) else {
        return fallback;
    };
    let mut position = 0;
    while let Some(value) = find_key(input, position, key) {
        position = value;
        let mut index = skip_space(input, value);
        if input.get(index) == Some(&b'"') {
            index += 1;
        }
        if input
            .get(index)
            .is_some_and(|byte| byte.is_ascii_digit() || matches!(byte, b'-' | b'.'))
        {
            let mut stop = index;
            while input.get(stop).is_some_and(|byte| {
                byte.is_ascii_digit() || matches!(byte, b'-' | b'+' | b'.' | b'e' | b'E')
            }) {
                stop += 1;
            }
            let length = stop - index;
            if length >= 128 {
                return fallback;
            }
            let mut number = [0u8; 128];
            number[..length].copy_from_slice(&input[index..stop]);
            let mut parsed_end = ptr::null_mut();
            let parsed = unsafe { strtod(number.as_ptr().cast(), &mut parsed_end) };
            return if parsed_end.cast_const() != number.as_ptr().cast() && parsed.is_finite() {
                parsed
            } else {
                fallback
            };
        }
    }
    fallback
}

/// Return the first object/string element of a named array.
/// # Safety
/// Readable inputs follow `rust_core.h`; returned pointers borrow the input.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_array(
    start: *const c_char,
    end: *const c_char,
    key: *const c_char,
) -> *const c_char {
    let (Some(input), Some(key)) = (unsafe { input_range(start, end) }, unsafe {
        input_range(key, ptr::null())
    }) else {
        return ptr::null();
    };
    let element = find_key(input, 0, key)
        .map(|index| skip_space(input, index))
        .filter(|&index| input.get(index) == Some(&b'['))
        .and_then(|index| first_element(input, index + 1));
    element.map_or(ptr::null(), |index| unsafe { start.add(index) })
}

/// Return an object/string after a comma following the previous element.
/// # Safety
/// Non-null input is NUL-terminated; returned pointers borrow it.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_next(input: *const c_char) -> *const c_char {
    if input.is_null() {
        return ptr::null();
    }
    let current = unsafe { skip_space_pointer(input) };
    if unsafe { *current.cast::<u8>() } != b',' {
        return ptr::null();
    }
    unsafe { first_element_pointer(current.add(1)) }
}

/// Return the first object/string of a root array.
/// # Safety
/// Non-null input is NUL-terminated; returned pointers borrow it.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_root_array(input: *const c_char) -> *const c_char {
    if input.is_null() {
        return ptr::null();
    }
    let current = unsafe { skip_space_pointer(input) };
    if unsafe { *current.cast::<u8>() } != b'[' {
        return ptr::null();
    }
    unsafe { first_element_pointer(current.add(1)) }
}

// SAFETY: input is non-null and readable through its NUL terminator.
unsafe fn skip_space_pointer(mut input: *const c_char) -> *const c_char {
    loop {
        let byte = unsafe { *input.cast::<u8>() };
        if byte == 0 || byte > b' ' {
            return input;
        }
        input = unsafe { input.add(1) };
    }
}

unsafe fn first_element_pointer(input: *const c_char) -> *const c_char {
    let input = unsafe { skip_space_pointer(input) };
    if matches!(unsafe { *input.cast::<u8>() }, b'{' | b'"') {
        input
    } else {
        ptr::null()
    }
}

/// Copy a complete raw JSON value, preserving output on failure.
/// # Safety
/// Inputs and disjoint output follow the contracts in `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_raw(
    start: *const c_char,
    end: *const c_char,
    key: *const c_char,
    output: *mut c_char,
    capacity: usize,
) -> i32 {
    let (Some(input), Some(key), Some(output)) = (
        unsafe { input_range(start, end) },
        unsafe { input_range(key, ptr::null()) },
        unsafe { output_range(output, capacity) },
    ) else {
        return 0;
    };
    let Some(index) = find_key(input, 0, key)
        .map(|index| skip_space(input, index))
        .filter(|&index| index < input.len())
    else {
        return 0;
    };
    let stop = match input[index] {
        b'{' | b'[' => container_end(input, index),
        b'"' => string_end(input, index).map(|index| index + 1),
        _ => {
            let mut stop = index;
            while input
                .get(stop)
                .is_some_and(|byte| !matches!(byte, 0 | b',' | b'}' | b']'))
            {
                stop += 1;
            }
            while stop > index && input[stop - 1] <= b' ' {
                stop -= 1;
            }
            Some(stop)
        }
    };
    let Some(stop) = stop else {
        return 0;
    };
    let length = stop - index;
    if length >= output.len() {
        return 0;
    }
    output[..length].copy_from_slice(&input[index..stop]);
    output[length] = 0;
    1
}

/// Parse the established UTC timestamp format, using the native libc range.
/// # Safety
/// Non-null input must be readable and NUL-terminated throughout this call.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_iso_ms(input: *const c_char) -> i64 {
    let Some(bytes) =
        (unsafe { input_range(input, ptr::null()) }).filter(|bytes| !bytes.is_empty())
    else {
        return 0;
    };
    let (mut year, mut month, mut day, mut hour, mut minute, mut second) = (
        0 as c_int, 0 as c_int, 0 as c_int, 0 as c_int, 0 as c_int, 0 as c_int,
    );
    let mut separator = 0 as c_char;
    let conversions = unsafe {
        scan_date(
            input,
            c"%d-%d-%d%c%d:%d:%d".as_ptr(),
            &mut year as *mut c_int,
            &mut month as *mut c_int,
            &mut day as *mut c_int,
            &mut separator as *mut c_char,
            &mut hour as *mut c_int,
            &mut minute as *mut c_int,
            &mut second as *mut c_int,
        )
    };
    if conversions < 3 {
        return 0;
    }
    let (Some(year), Some(month)) = (year.checked_sub(1900), month.checked_sub(1)) else {
        return 0;
    };
    let mut time = NativeTime {
        second,
        minute,
        hour,
        day,
        month,
        year,
        weekday: 0,
        year_day: 0,
        daylight: 0,
        utc_offset: 0,
        zone: ptr::null(),
    };
    let seconds = unsafe { timegm(&mut time) };
    if seconds < 0 {
        return 0;
    }
    let mut fraction = 0;
    if let Some(dot) = bytes.iter().position(|&byte| byte == b'.') {
        let mut digits = 0;
        for &byte in bytes[dot + 1..]
            .iter()
            .take(3)
            .take_while(|byte| byte.is_ascii_digit())
        {
            fraction = fraction * 10 + i64::from(byte - b'0');
            digits += 1;
        }
        for _ in digits..3 {
            fraction *= 10;
        }
    }
    (seconds as i64)
        .checked_mul(1000)
        .and_then(|value| value.checked_add(fraction))
        .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn escaped_and_literal_characters_keep_the_same_prefix() {
        for (escaped, literal) in [
            (br#""\u00e9AB""#.as_slice(), "\"éAB\"".as_bytes()),
            (br#""\u65e5AB""#, "\"日AB\"".as_bytes()),
            (br#""x\ud83d\ude00AB""#, "\"x😀AB\"".as_bytes()),
        ] {
            for capacity in 1..12 {
                let mut first = [0x55; 16];
                let mut second = first;
                assert_eq!(
                    read_string(escaped, 0, &mut first[..capacity], false),
                    read_string(literal, 0, &mut second[..capacity], false)
                );
                let first_end = first.iter().position(|&byte| byte == 0).unwrap();
                let second_end = second.iter().position(|&byte| byte == 0).unwrap();
                assert_eq!(&first[..=first_end], &second[..=second_end]);
                assert!(first[capacity..].iter().all(|&byte| byte == 0x55));
                assert!(second[capacity..].iter().all(|&byte| byte == 0x55));
            }
        }
    }

    #[test]
    fn tolerant_escapes_and_lines_are_preserved() {
        let mut output = [0; 80];
        assert_eq!(
            read_string(br#""a\n\r\t\q\b\f\/\uZZ\ud800""#, 0, &mut output, true),
            1
        );
        assert_eq!(&output[..14], b"a\n qbf/ ZZ \0\0\0");
        output.fill(0x55);
        assert_eq!(read_string(br#""\u0000x""#, 0, &mut output, false), 1);
        assert_eq!(&output[..3], b"\0x\0");
    }

    #[test]
    fn bounded_lookup_and_root_keys_differ() {
        let input = br#"{"nested":{"name":"inner"},"name":"outer"}"#;
        let first = find_key(input, 0, b"name").unwrap();
        let root = root_key(input, b"name").unwrap();
        let mut output = [0; 16];
        read_string(input, first, &mut output, false);
        assert_eq!(&output[..6], b"inner\0");
        read_string(input, root, &mut output, false);
        assert_eq!(&output[..6], b"outer\0");
        assert_eq!(string_end(&input[..first + 3], first), None);
    }

    #[test]
    fn numeric_and_timestamp_native_conversion() {
        unsafe {
            assert_eq!(
                nuvio_rust_json_number(
                    c"{\"n\":{\"n\":\"8.1\"}}".as_ptr(),
                    ptr::null(),
                    c"n".as_ptr(),
                    -1.0
                ),
                8.1
            );
            assert_eq!(
                nuvio_rust_json_number(c"{\"n\":1e999}".as_ptr(), ptr::null(), c"n".as_ptr(), -1.0),
                -1.0
            );
            assert_eq!(
                nuvio_rust_json_iso_ms(c"1970-01-01T00:00:00.123456Z".as_ptr()),
                123
            );
            assert_eq!(
                nuvio_rust_json_iso_ms(c"2026-09-04T18:52:07.123456+00:00".as_ptr()),
                1_788_547_927_123
            );
            assert_eq!(nuvio_rust_json_iso_ms(c"1969-12-31T23:59:59Z".as_ptr()), 0);
        }
    }
}
