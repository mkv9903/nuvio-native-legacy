//! Provider text cleanup using the existing bundled-font and separator rules.

use crate::{stylized::fold_stylized_character, utf8};
use std::ffi::{c_char, CStr};

const SINGLE_LINE: i32 = 1;
// Country/language codes are compatibility data, including non-English audio.
const FLAG_LANGUAGES: &[u8] = b"GBENUSENAUENCAENIEENBRPTPTPTESESMXESARESCOESCLESFRFRBEFRDEDEATDECHDEITITNLNLRURUUAUKPLPLTRTRJPJAKRKOCNZHTWZHHKZHINHISAARAEAREGARSESVNONODKDAFIFIGRELCZCSHUHUROROILHETHTHVNVIIDID";

#[derive(Clone, Copy, PartialEq)]
enum Kind {
    Visible,
    Space,
    Newline,
    Separator,
    Drop,
    Flag,
}

fn classify(cp: u32) -> (Kind, Option<u32>) {
    use Kind::*;
    if cp < 0x80 {
        return (
            match cp {
                10 => Newline,
                9 | 11 | 12 | 32 => Space,
                0..=31 | 127 => Drop,
                _ => Visible,
            },
            None,
        );
    }
    let folded = fold_stylized_character(cp);
    if folded != 0 {
        return (Visible, Some(folded.into()));
    }
    let kind = if cp == 0xfffd || cp < 0xa0 {
        Drop
    } else if cp == 0xa0 {
        Space
    } else if cp == 0xad {
        Drop
    } else if cp < 0x300 {
        Visible
    } else if cp <= 0x36f {
        if cp == 0x34f || (0x332..=0x338).contains(&cp) {
            Drop
        } else {
            Visible
        }
    } else if cp < 0x2000 {
        if cp == 0x180e {
            Drop
        } else {
            Visible
        }
    } else if cp <= 0x206f {
        if cp <= 0x200a || cp == 0x202f || cp == 0x205f {
            Space
        } else if cp <= 0x200f || (0x202a..=0x202e).contains(&cp) || cp >= 0x2060 {
            Drop
        } else if cp == 0x2028 || cp == 0x2029 {
            Newline
        } else {
            let replacement = match cp {
                0x2010..=0x2012 => Some(0x2d),
                0x2015 => Some(0x2014),
                0x2016 => Some(0x7c),
                0x2023 => Some(0x2022),
                _ => None,
            };
            if replacement.is_some() {
                return (Visible, replacement);
            }
            match cp {
                0x2013 | 0x2014 | 0x2018 | 0x2019 | 0x201a | 0x201c | 0x201d | 0x201e | 0x2020
                | 0x2021 | 0x2022 | 0x2026 | 0x2030 | 0x2032 | 0x2033 | 0x2039 | 0x203a => Visible,
                _ => Separator,
            }
        }
    } else if cp <= 0x2bff {
        let replacement = match cp {
            0x2705 | 0x2714 | 0x2611 => Some(0x2713),
            0x274c | 0x274e | 0x2716 | 0x2715 | 0x2717 | 0x2718 | 0x2612 => Some(0xd7),
            0x2b50 => Some(0x2605),
            0x27a1 | 0x279c | 0x2794 | 0x27a4 | 0x27a2 | 0x2799 | 0x279e | 0x2b95 | 0x21d2 => {
                Some(0x2192)
            }
            _ => None,
        };
        if replacement.is_some() {
            return (Visible, replacement);
        }
        match cp {
            0x20ac
            | 0x2116
            | 0x2122
            | 0x2190
            | 0x2191
            | 0x2192
            | 0x2193
            | 0x2212
            | 0x2248
            | 0x2264
            | 0x2265
            | 0x221e
            | 0x2605
            | 0x2713
            | 0x2150..=0x215e => Visible,
            0x20a0..=0x20cf | 0x2190..=0x21ff | 0x20d0..=0x20ff => Drop,
            _ => Separator,
        }
    } else if (0x2e00..=0x2e7f).contains(&cp) {
        Drop
    } else if cp < 0x3000 {
        Visible
    } else if cp == 0x3000 {
        Space
    } else if cp < 0xd800 {
        Visible
    } else if cp < 0xf900 {
        Separator
    } else if (0xfe00..=0xfe0f).contains(&cp)
        || (0xfe20..=0xfe2f).contains(&cp)
        || cp == 0xfeff
        || (0xfff0..0x10000).contains(&cp)
    {
        Drop
    } else if cp < 0x10000 {
        Visible
    } else if (0x1f1e6..=0x1f1ff).contains(&cp) {
        Flag
    } else if (0x1f3fb..=0x1f3ff).contains(&cp) {
        Drop
    } else if cp == 0x1f31f {
        return (Visible, Some(0x2605));
    } else if (0x1f000..=0x1fbff).contains(&cp) {
        Separator
    } else {
        Drop
    };
    (kind, None)
}

fn flag_language(country: [u8; 2]) -> [u8; 2] {
    FLAG_LANGUAGES
        .chunks_exact(4)
        .find(|entry| entry[..2] == country)
        .map_or(country, |entry| [entry[2], entry[3]])
}

fn cleanup(input: &[u8], output: &mut [u8], flags: i32) -> usize {
    let mut read = 0;
    let mut written = 0;
    let mut pending = 0;
    let mut line_break = false;
    let mut last_byte = 0;
    let mut last_flag = false;
    let mut empty_line = true;
    while read < input.len() {
        let start = read;
        let (cp, consumed) = utf8::decode(&input[read..]);
        read += consumed;
        let (mut kind, replacement) = classify(cp);
        let mut mapped = [0; 4];
        let mut mapped_length = replacement.map_or(0, |cp| utf8::encode(cp, &mut mapped));
        let is_flag = kind == Kind::Flag;
        if is_flag {
            let first = b'A' + (cp - 0x1f1e6) as u8;
            let (next, width) = utf8::decode(&input[read..]);
            if (0x1f1e6..=0x1f1ff).contains(&next) {
                let country = [first, b'A' + (next - 0x1f1e6) as u8];
                mapped[..2].copy_from_slice(&flag_language(country));
                mapped_length = 2;
                read += width;
            } else {
                mapped[0] = first;
                mapped_length = 1;
            }
            kind = Kind::Visible;
            if last_flag && pending == 0 {
                pending = 1;
            }
        } else {
            last_flag = false;
        }
        match kind {
            Kind::Drop => continue,
            Kind::Space => {
                if pending == 0 {
                    pending = 1;
                }
                continue;
            }
            Kind::Newline => {
                if flags & SINGLE_LINE != 0 {
                    if !empty_line {
                        pending = 2;
                    }
                } else if !empty_line {
                    line_break = true;
                    pending = 0;
                    empty_line = true;
                    last_byte = 0;
                }
                continue;
            }
            Kind::Separator => {
                if !empty_line {
                    pending = 2;
                }
                continue;
            }
            _ => {}
        }
        let text = if mapped_length > 0 {
            &mapped[..mapped_length]
        } else {
            &input[start..start + consumed]
        };
        let first = text[0];
        let separator = first == b'|' || text == b"\xc2\xb7";
        let mut prefix = [0; 4];
        let prefix_length = if line_break {
            prefix[0] = b'\n';
            line_break = false;
            1
        } else if !empty_line && pending == 2 {
            if b")]},.;:!?".contains(&first) {
                0
            } else if separator
                || [b'|', 0xc2].contains(&last_byte)
                || b"([{:/-".contains(&last_byte)
            {
                prefix[0] = b' ';
                1
            } else {
                prefix = *b" \xc2\xb7 ";
                4
            }
        } else if !empty_line && pending != 0 {
            prefix[0] = b' ';
            1
        } else {
            0
        };
        if prefix_length + text.len() >= output.len() - written {
            break;
        }
        output[written..written + prefix_length].copy_from_slice(&prefix[..prefix_length]);
        written += prefix_length;
        output[written..written + text.len()].copy_from_slice(text);
        written += text.len();
        pending = 0;
        empty_line = false;
        last_flag = is_flag;
        last_byte = if text[text.len() - 1] < 0x80 {
            text[text.len() - 1]
        } else if text == b"\xc2\xb7" {
            0xc2
        } else {
            0
        };
    }
    output[written] = 0;
    // Remove empty decoration-only ()/[] pairs, preserving legacy spacing.
    let mut read = 0;
    let mut compacted = 0;
    while read < written {
        if (output[read] == b'(' && output[read + 1] == b')')
            || (output[read] == b'[' && output[read + 1] == b']')
        {
            if compacted > 0
                && output[compacted - 1] == b' '
                && matches!(output[read + 2], b' ' | 0)
            {
                compacted -= 1;
            }
            read += 2;
        } else {
            output[compacted] = output[read];
            compacted += 1;
            read += 1;
        }
    }
    output[compacted] = 0;
    compacted
}

fn separator_length(bytes: &[u8]) -> usize {
    if bytes.first().is_some_and(|b| b"|-/,".contains(b)) {
        1
    } else if bytes.starts_with(b"\xe2\x80\xa2") {
        3
    } else if bytes.starts_with(b"\xc2\xb7") {
        2
    } else {
        0
    }
}

fn only_separators(mut bytes: &[u8]) -> bool {
    if bytes.is_empty() {
        return false;
    }
    while !bytes.is_empty() {
        let width = separator_length(bytes);
        if width == 0 {
            return false;
        }
        bytes = &bytes[width..];
    }
    true
}

fn trim_separators(buffer: &mut [u8], length: usize) -> usize {
    let mut read = 0;
    let mut written = 0;
    let mut last_separator = true;
    let mut pending = None;
    while read < length {
        while read < length && matches!(buffer[read], b' ' | b'\t') {
            read += 1;
        }
        if read == length {
            break;
        }
        let mut start = read;
        while read < length && !matches!(buffer[read], b' ' | b'\t') {
            read += 1;
        }
        let mut end = read;
        if only_separators(&buffer[start..end]) {
            if !last_separator || pending.is_some() {
                pending = Some((start, end));
            }
            last_separator = true;
            continue;
        }
        if written == 0 {
            while start < end {
                let width = separator_length(&buffer[start..end]);
                if width == 0 {
                    break;
                }
                start += width;
            }
        }
        let mut next = end;
        while next < length && matches!(buffer[next], b' ' | b'\t') {
            next += 1;
        }
        if next == length {
            loop {
                let mut cursor = start;
                let mut final_separator = None;
                while cursor < end {
                    let width = separator_length(&buffer[cursor..end]);
                    if width != 0 && cursor + width == end {
                        final_separator = Some(cursor);
                        break;
                    }
                    cursor += if width == 0 { 1 } else { width };
                }
                let Some(trimmed) = final_separator else {
                    break;
                };
                end = trimmed;
                if end == start {
                    break;
                }
            }
        }
        if start >= end {
            continue;
        }
        if let Some((start, end)) = pending {
            if written != 0 {
                buffer[written] = b' ';
                written += 1;
                buffer.copy_within(start..end, written);
                written += end - start;
            }
        }
        if written != 0 {
            buffer[written] = b' ';
            written += 1;
        }
        buffer.copy_within(start..end, written);
        written += end - start;
        pending = None;
        last_separator = false;
    }
    buffer[written] = 0;
    written
}

/// Clean provider text, returning the byte length excluding the NUL.
///
/// # Safety
/// Input is NUL-terminated bytes or null. Output is writable for `capacity`
/// bytes or null. Buffers must not overlap; no pointers/allocations are retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_text_cleanup(
    input: *const c_char,
    output: *mut c_char,
    capacity: usize,
    flags: i32,
) -> usize {
    if output.is_null() || capacity == 0 {
        return 0;
    }
    let input = if input.is_null() {
        &[]
    } else {
        unsafe { CStr::from_ptr(input) }.to_bytes()
    };
    let output = unsafe { std::slice::from_raw_parts_mut(output.cast::<u8>(), capacity) };
    cleanup(input, output, flags)
}

/// Tidy separators in one already-cleaned line in place.
///
/// # Safety
/// Buffer is a readable/writable NUL-terminated string or null. Null returns
/// zero. Output never grows; the buffer is not retained.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_text_trim_separators(buffer: *mut c_char) -> usize {
    if buffer.is_null() {
        return 0;
    }
    let length = unsafe { CStr::from_ptr(buffer) }.to_bytes().len();
    let bytes = unsafe { std::slice::from_raw_parts_mut(buffer.cast::<u8>(), length + 1) };
    trim_separators(bytes, length)
}

#[cfg(test)]
mod tests {
    use super::*;
    fn clean(input: &str, capacity: usize, flags: i32) -> Vec<u8> {
        let mut output = vec![0x55; capacity];
        let length = cleanup(input.as_bytes(), &mut output, flags);
        output.truncate(length);
        output
    }
    #[test]
    fn flags_decorations_and_line_modes() {
        assert_eq!(
            clean("🇬🇧🇧🇷\n✅ ＨＤＲ (🌐)", 128, SINGLE_LINE),
            "EN PT · ✓ HDR".as_bytes()
        );
        assert_eq!(clean("A\n\n B", 32, 0), b"A\nB");
        assert_eq!(clean("日本 Друзья", 64, 0), "日本 Друзья".as_bytes());
    }
    #[test]
    fn capacity_never_splits_a_scalar_or_separator_prefix() {
        for capacity in 1..32 {
            let output = clean("日本 ⚡ ✓", capacity, SINGLE_LINE);
            assert!(std::str::from_utf8(&output).is_ok());
            assert!(output.len() < capacity);
            assert!(!output.ends_with(b" "));
        }
    }
    #[test]
    fn parallel_stylized_cleanup_has_no_shared_replacement_buffer() {
        std::thread::scope(|scope| {
            for (input, expected) in [("ＨＤＲ", "HDR"), ("ᴴᴰ", "HD"), ("Ⓓⓤⓑ", "Dub")]
            {
                scope.spawn(move || {
                    for _ in 0..1000 {
                        assert_eq!(clean(input, 32, 0), expected.as_bytes());
                    }
                });
            }
        });
    }
}
