//! Fold provider-styled letters to glyphs supported by the bundled fonts.

use std::ffi::{c_char, c_ulong};

const SMALL_CAPS: &[(u32, u8)] = &[
    (0x1d00, b'A'),
    (0x0299, b'B'),
    (0x1d04, b'C'),
    (0x1d05, b'D'),
    (0x1d07, b'E'),
    (0xa730, b'F'),
    (0x0262, b'G'),
    (0x029c, b'H'),
    (0x026a, b'I'),
    (0x1d0a, b'J'),
    (0x1d0b, b'K'),
    (0x029f, b'L'),
    (0x1d0d, b'M'),
    (0x0274, b'N'),
    (0x1d0f, b'O'),
    (0x1d18, b'P'),
    (0x01eb, b'Q'),
    (0x0280, b'R'),
    (0xa731, b'S'),
    (0x1d1b, b'T'),
    (0x1d1c, b'U'),
    (0x1d20, b'V'),
    (0x1d21, b'W'),
    (0x028f, b'Y'),
    (0x1d22, b'Z'),
];
const MODIFIERS: &[(u32, u8)] = &[
    (0x1d2c, b'A'),
    (0x1d2e, b'B'),
    (0x1d30, b'D'),
    (0x1d31, b'E'),
    (0x1d33, b'G'),
    (0x1d34, b'H'),
    (0x1d35, b'I'),
    (0x1d36, b'J'),
    (0x1d37, b'K'),
    (0x1d38, b'L'),
    (0x1d39, b'M'),
    (0x1d3a, b'N'),
    (0x1d3c, b'O'),
    (0x1d3e, b'P'),
    (0x1d3f, b'R'),
    (0x1d40, b'T'),
    (0x1d41, b'U'),
    (0x2c7d, b'V'),
    (0x1d42, b'W'),
    (0x1d43, b'a'),
    (0x1d47, b'b'),
    (0x1d9c, b'c'),
    (0x1d48, b'd'),
    (0x1d49, b'e'),
    (0x1da0, b'f'),
    (0x1d4d, b'g'),
    (0x02b0, b'h'),
    (0x2071, b'i'),
    (0x02b2, b'j'),
    (0x1d4f, b'k'),
    (0x02e1, b'l'),
    (0x1d50, b'm'),
    (0x207f, b'n'),
    (0x1d52, b'o'),
    (0x1d56, b'p'),
    (0x02b3, b'r'),
    (0x02e2, b's'),
    (0x1d57, b't'),
    (0x1d58, b'u'),
    (0x1d5b, b'v'),
    (0x02b7, b'w'),
    (0x02e3, b'x'),
    (0x02b8, b'y'),
    (0x1dbb, b'z'),
];

pub fn fold_stylized_character(codepoint: u32) -> u8 {
    if codepoint < 0x80 {
        return 0;
    }
    if let Some(&(_, letter)) = SMALL_CAPS.iter().find(|&&(cp, _)| cp == codepoint) {
        return letter;
    }
    match codepoint {
        0x2070 => b'0',
        0x2074..=0x2079 => b'4' + (codepoint - 0x2074) as u8,
        0x2080..=0x2089 => b'0' + (codepoint - 0x2080) as u8,
        0x24b6..=0x24cf => b'A' + (codepoint - 0x24b6) as u8,
        0x24d0..=0x24e9 => b'a' + (codepoint - 0x24d0) as u8,
        0x2460..=0x2468 => b'1' + (codepoint - 0x2460) as u8,
        0xff01..=0xff5e => (codepoint - 0xfee0) as u8,
        // Retain the legacy arithmetic mapping, including unassigned holes.
        0x1d400..=0x1d6a3 => {
            let offset = ((codepoint - 0x1d400) % 52) as u8;
            if offset < 26 {
                b'A' + offset
            } else {
                b'a' + offset - 26
            }
        }
        0x1d7ce..=0x1d7ff => b'0' + ((codepoint - 0x1d7ce) % 10) as u8,
        0x1f130..=0x1f149 => b'A' + (codepoint - 0x1f130) as u8,
        0x1f150..=0x1f169 => b'A' + (codepoint - 0x1f150) as u8,
        0x1f170..=0x1f189 => b'A' + (codepoint - 0x1f170) as u8,
        _ => MODIFIERS
            .iter()
            .find(|&&(cp, _)| cp == codepoint)
            .map_or(0, |&(_, letter)| letter),
    }
}

/// ASCII equivalent, or zero. `c_ulong` matches both ARM32 and host C callers.
#[no_mangle]
pub extern "C" fn nuvio_rust_fold_stylized_character(codepoint: c_ulong) -> c_char {
    u32::try_from(codepoint).map_or(0, fold_stylized_character) as c_char
}
