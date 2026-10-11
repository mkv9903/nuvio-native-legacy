//! Bounded UTF-8 decoding shared by cleanup and local search.

/// Invalid encodings consume one byte, including overlongs and surrogates.
pub fn decode(input: &[u8]) -> (u32, usize) {
    let Some(&first) = input.first() else {
        return (0, 0);
    };
    let (mut codepoint, width) = match first {
        0..=0x7f => return (u32::from(first), 1),
        0xc2..=0xdf => (u32::from(first & 0x1f), 2),
        0xe0..=0xef => (u32::from(first & 0x0f), 3),
        0xf0..=0xf4 => (u32::from(first & 7), 4),
        _ => return (0xfffd, 1),
    };
    let Some(bytes) = input.get(1..width) else {
        return (0xfffd, 1);
    };
    for &byte in bytes {
        if byte & 0xc0 != 0x80 {
            return (0xfffd, 1);
        }
        codepoint = (codepoint << 6) | u32::from(byte & 0x3f);
    }
    if (width == 3 && codepoint < 0x800)
        || (width == 4 && !(0x10000..=0x10ffff).contains(&codepoint))
        || (0xd800..=0xdfff).contains(&codepoint)
    {
        return (0xfffd, 1);
    }
    (codepoint, width)
}

pub fn encode(codepoint: u32, output: &mut [u8; 4]) -> usize {
    // All callers supply a scalar from the strict decoder or an ASCII mapping.
    char::from_u32(codepoint)
        .unwrap_or('\u{fffd}')
        .encode_utf8(output)
        .len()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn malformed_sequences_consume_one_byte() {
        for input in [
            b"\xe0\x80\x80".as_slice(),
            b"\xed\xa0\x80",
            b"\xf0\x80\x80\x80",
            b"\xf4\x90\x80\x80",
            b"\xc2",
            b"\xe2\x28\xa1",
            b"\xff",
        ] {
            assert_eq!(decode(input), (0xfffd, 1));
        }
    }

    #[test]
    fn scalar_boundaries_round_trip() {
        for codepoint in [
            1, 0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x10ffff,
        ] {
            let mut bytes = [0; 4];
            let width = encode(codepoint, &mut bytes);
            assert_eq!(decode(&bytes[..width]), (codepoint, width));
        }
    }
}
