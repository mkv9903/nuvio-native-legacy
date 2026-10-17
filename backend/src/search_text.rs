//! Search text uses Rust UTF-8 handling and canonical Unicode decomposition.
//! Provider requests use the original input; folding is only for local matching.
use unicode_normalization::{char::is_combining_mark, UnicodeNormalization};

/// Repair each invalid byte independently, without accepting overlong encodings,
/// surrogate values or truncated Unicode sequences as text.
pub fn decode_input(bytes: &[u8]) -> String {
    let mut text = String::with_capacity(bytes.len());
    for chunk in bytes.utf8_chunks() {
        text.push_str(chunk.valid());
        text.extend(std::iter::repeat(' ').take(chunk.invalid().len()));
    }
    text
}

pub fn clean_query(text: &str) -> String {
    text.trim()
        .chars()
        .map(|ch| if ch.is_control() { ' ' } else { ch })
        .collect()
}

pub fn local_key(text: &str) -> String {
    let mut folded = String::with_capacity(text.len());
    let mut latin = false;
    for ch in text.chars() {
        if is_combining_mark(ch) && latin {
            continue;
        }
        latin = ch.is_ascii_alphabetic()
            || (ch.is_alphabetic() && matches!(ch as u32, 0x00c0..=0x024f | 0x1e00..=0x1eff));
        if latin {
            folded.extend(ch.to_lowercase().nfd().filter(|ch| !is_combining_mark(*ch)));
        } else {
            folded.extend(ch.to_lowercase().map(|ch| if ch == 'ё' { 'е' } else { ch }));
        }
    }
    folded
}

/// History compares spelling/case, not accent-insensitive search equivalence.
pub fn history_key(text: &str) -> String {
    text.nfc().flat_map(char::to_lowercase).collect()
}

pub fn prefix(text: &str, bytes: usize) -> &str {
    let mut end = bytes.min(text.len());
    while !text.is_char_boundary(end) {
        end -= 1;
    }
    &text[..end]
}

pub fn character_count(bytes: &[u8]) -> usize {
    bytes
        .utf8_chunks()
        .map(|chunk| chunk.valid().chars().count() + chunk.invalid().len())
        .sum()
}

pub fn last_character_start(bytes: &[u8]) -> usize {
    let mut offset = 0;
    let mut last = 0;
    for chunk in bytes.utf8_chunks() {
        if let Some((index, _)) = chunk.valid().char_indices().next_back() {
            last = offset + index;
        }
        offset += chunk.valid().len();
        if !chunk.invalid().is_empty() {
            offset += chunk.invalid().len();
            last = offset - 1;
        }
    }
    last
}

#[derive(Clone, Default, Debug, PartialEq, Eq)]
pub struct SearchText(String);
impl SearchText {
    pub fn set(&mut self, bytes: &[u8]) {
        self.0 = decode_input(bytes)
            .chars()
            .map(|ch| if ch.is_control() { ' ' } else { ch })
            .collect();
    }
    pub fn append(&mut self, bytes: &[u8]) {
        self.0
            .extend(decode_input(bytes).chars().filter(|ch| !ch.is_control()));
    }
    pub fn backspace(&mut self) {
        self.0.pop();
    }
    pub fn clear(&mut self) {
        self.0.clear();
    }
    pub fn text(&self) -> &str {
        &self.0
    }
    pub fn searchable(&self) -> bool {
        local_key(self.0.trim()).chars().count() >= 2
    }
}
