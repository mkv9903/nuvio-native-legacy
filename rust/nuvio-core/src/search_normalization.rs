//! Local matching and bounded editing; provider search terms remain unchanged.

use crate::utf8;
use std::ffi::{c_char, CStr};

const LATIN_EXTENDED_A: &[u8; 128] = b"aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiii  jjkkkllllll  llnnnnnnnnnoooooo  rrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";

fn fold(codepoint: u32) -> u32 {
    match codepoint {
        0x41..=0x5a => codepoint + 32,
        0xc0..=0xc6 | 0xe0..=0xe6 => b'a'.into(),
        0xc7 | 0xe7 => b'c'.into(),
        0xc8..=0xcb | 0xe8..=0xeb => b'e'.into(),
        0xcc..=0xcf | 0xec..=0xef => b'i'.into(),
        0xd0 | 0xf0 => b'd'.into(),
        0xd1 | 0xf1 => b'n'.into(),
        0xd2..=0xd6 | 0xd8 | 0xf2..=0xf6 | 0xf8 => b'o'.into(),
        0xd9..=0xdc | 0xf9..=0xfc => b'u'.into(),
        0xdd | 0xfd | 0xff => b'y'.into(),
        0xc0..=0xff => b' '.into(),
        0x100..=0x17f => LATIN_EXTENDED_A[(codepoint - 0x100) as usize].into(),
        0x218 | 0x219 => b's'.into(),
        0x21a | 0x21b => b't'.into(),
        0x410..=0x42f => codepoint + 0x20,
        0x401 | 0x451 => 0x435,
        0x400..=0x40f => codepoint + 0x50,
        0x490..=0x4bf if codepoint & 1 == 0 => codepoint + 1,
        _ => codepoint,
    }
}

/// Lowercase/fold local search text, replacing each invalid byte with a space.
///
/// # Safety
/// Input is readable NUL-terminated bytes or null. Output is writable for
/// `capacity` bytes or null. Buffers are disjoint or have exactly the same
/// address; partial overlap is unsupported. No pointer is retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_search_normalize(
    input: *const c_char,
    output: *mut c_char,
    capacity: usize,
) {
    if output.is_null() || capacity == 0 {
        return;
    }
    let length = if input.is_null() {
        0
    } else {
        unsafe { CStr::from_ptr(input) }.to_bytes().len()
    };
    let mut read = 0;
    let mut written = 0;
    while read < length {
        // End the input borrow before writing: exact in-place normalization
        // is safe because every replacement is no longer than consumed bytes.
        let (codepoint, consumed, invalid) = {
            let bytes =
                unsafe { std::slice::from_raw_parts(input.cast::<u8>().add(read), length - read) };
            let (cp, width) = utf8::decode(bytes);
            (cp, width, cp == 0xfffd && width == 1)
        };
        let mut bytes = [0; 4];
        let width = utf8::encode(
            if invalid {
                b' '.into()
            } else {
                fold(codepoint)
            },
            &mut bytes,
        );
        if width >= capacity - written {
            break;
        }
        unsafe {
            std::ptr::copy_nonoverlapping(bytes.as_ptr(), output.cast::<u8>().add(written), width);
        }
        written += width;
        read += consumed;
    }
    unsafe {
        output.add(written).write(0);
    }
}

/// Count non-continuation bytes, preserving the legacy malformed-input rule.
///
/// # Safety
/// Input is a readable NUL-terminated byte string or null, with a count that
/// fits a C int. No pointer is retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_search_codepoints(input: *const c_char) -> i32 {
    if input.is_null() {
        return 0;
    }
    unsafe { CStr::from_ptr(input) }
        .to_bytes()
        .iter()
        .filter(|&&byte| byte & 0xc0 != 0x80)
        .count() as i32
}

/// Delete the final character in place and return the new byte length.
///
/// # Safety
/// A non-null buffer is readable/writable through `length` inclusive, with
/// `length` the current byte length. Null returns zero. No pointer is retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_search_delete_last(
    buffer: *mut c_char,
    mut length: usize,
) -> usize {
    if buffer.is_null() {
        return 0;
    }
    while length > 0 {
        length -= 1;
        if unsafe { buffer.cast::<u8>().add(length).read() } & 0xc0 != 0x80 {
            break;
        }
    }
    unsafe {
        buffer.add(length).write(0);
    }
    length
}

#[cfg(test)]
mod tests {
    use super::*;
    fn normalize(input: &[u8], capacity: usize) -> Vec<u8> {
        let mut input = input.to_vec();
        input.push(0);
        let mut output = vec![0x55; capacity];
        unsafe {
            nuvio_rust_search_normalize(
                input.as_ptr().cast(),
                output.as_mut_ptr().cast(),
                capacity,
            );
        }
        output.truncate(output.iter().position(|&b| b == 0).unwrap_or(capacity));
        output
    }
    #[test]
    fn approved_malformed_utf8_fix_and_literal_replacement_character() {
        for malformed in [
            b"\xe0\x80\x80".as_slice(),
            b"\xed\xa0\x80",
            b"\xf0\x80\x80\x80",
            b"\xf4\x90\x80\x80",
        ] {
            assert_eq!(normalize(malformed, 16), vec![b' '; malformed.len()]);
        }
        assert_eq!(normalize("\u{fffd}".as_bytes(), 16), "\u{fffd}".as_bytes());
        assert_eq!(normalize(b"A\xe0\x80\x80Z", 16), b"a   z");
    }
    #[test]
    fn valid_scripts_and_small_buffers() {
        assert_eq!(
            normalize("ȘȚ ДРУЗЬЯ 日本".as_bytes(), 64),
            "st друзья 日本".as_bytes()
        );
        for capacity in 1..16 {
            let text = normalize("日本 Друзья".as_bytes(), capacity);
            assert!(std::str::from_utf8(&text).is_ok());
            assert!(text.len() < capacity);
        }
    }
    #[test]
    fn exact_in_place_normalization_keeps_unread_input() {
        let mut bytes = "ÉȘ ДРУЗЬЯ".as_bytes().to_vec();
        bytes.push(0);
        unsafe {
            nuvio_rust_search_normalize(
                bytes.as_ptr().cast(),
                bytes.as_mut_ptr().cast(),
                bytes.len(),
            );
        }
        assert_eq!(
            CStr::from_bytes_until_nul(&bytes).unwrap().to_bytes(),
            "es друзья".as_bytes()
        );
    }
}
