//! JSON emission using the legacy writer layout and native buffer ownership.
//! The C callers still initialize/free the context and borrow its output.

use std::ffi::{c_char, c_int, c_void, CStr};
use std::ptr;

const MAX_DEPTH: usize = 16;

/// Matches `Jsw` in `src/jsw.h`; field names stay English on the Rust side.
#[repr(C)]
pub struct JsonWriter {
    buffer: *mut c_char,
    length: usize,
    capacity: usize,
    error: c_int,
    depth: c_int,
    first: [c_char; MAX_DEPTH],
}

const _: () = {
    let pointer_size = std::mem::size_of::<*mut c_char>();
    assert!(std::mem::size_of::<c_int>() == 4);
    assert!(std::mem::size_of::<JsonWriter>() == if pointer_size == 8 { 48 } else { 36 });
    assert!(std::mem::offset_of!(JsonWriter, length) == pointer_size);
    assert!(std::mem::offset_of!(JsonWriter, capacity) == pointer_size * 2);
    assert!(std::mem::offset_of!(JsonWriter, error) == pointer_size * 3);
    assert!(std::mem::offset_of!(JsonWriter, depth) == pointer_size * 3 + 4);
    assert!(std::mem::offset_of!(JsonWriter, first) == pointer_size * 3 + 8);
};

extern "C" {
    fn realloc(buffer: *mut c_void, capacity: usize) -> *mut c_void;
    fn free(buffer: *mut c_void);
}

impl JsonWriter {
    fn empty() -> Self {
        Self {
            buffer: ptr::null_mut(),
            length: 0,
            capacity: 0,
            error: 0,
            depth: 0,
            first: [0; MAX_DEPTH],
        }
    }

    fn append(&mut self, bytes: &[u8]) {
        if self.error != 0 {
            return;
        }
        let Some(required) = self
            .length
            .checked_add(bytes.len())
            .and_then(|length| length.checked_add(1))
        else {
            self.error = 1;
            return;
        };
        if required > self.capacity {
            let mut capacity = if self.capacity == 0 {
                256
            } else {
                let Some(capacity) = self.capacity.checked_mul(2) else {
                    self.error = 1;
                    return;
                };
                capacity
            };
            while capacity < required {
                let Some(next) = capacity.checked_mul(2) else {
                    self.error = 1;
                    return;
                };
                capacity = next;
            }
            if capacity > isize::MAX as usize {
                self.error = 1;
                return;
            }
            let buffer = unsafe { realloc(self.buffer.cast(), capacity) };
            if buffer.is_null() {
                self.error = 1;
                return;
            }
            self.buffer = buffer.cast();
            self.capacity = capacity;
        }
        // Native allocation is owned by this initialized context. Inputs do
        // not alias the buffer, including across realloc's possible move.
        unsafe {
            ptr::copy_nonoverlapping(
                bytes.as_ptr(),
                self.buffer.cast::<u8>().add(self.length),
                bytes.len(),
            );
            self.length += bytes.len();
            self.buffer.add(self.length).write(0);
        }
    }

    fn separate(&mut self) {
        if self.error != 0 {
            return;
        }
        if self.depth > 0 && self.depth as usize <= MAX_DEPTH {
            let level = self.depth as usize - 1;
            if self.first[level] != 0 {
                self.first[level] = 0;
            } else {
                self.append(b",");
            }
        }
    }

    fn open(&mut self, delimiter: u8) {
        self.separate();
        self.append(&[delimiter]);
        if self.depth < 0 || self.depth as usize >= MAX_DEPTH {
            self.error = 1;
            return;
        }
        self.first[self.depth as usize] = 1;
        self.depth += 1;
    }

    fn close(&mut self, delimiter: u8) {
        if self.depth > 0 {
            self.depth -= 1;
        }
        self.append(&[delimiter]);
    }

    fn escaped(&mut self, input: &[u8]) {
        self.append(b"\"");
        for &byte in input {
            match byte {
                b'"' => self.append(b"\\\""),
                b'\\' => self.append(b"\\\\"),
                b'\n' => self.append(b"\\n"),
                b'\r' => self.append(b"\\r"),
                b'\t' => self.append(b"\\t"),
                0..=0x1f => {
                    const HEX: &[u8; 16] = b"0123456789abcdef";
                    self.append(&[
                        b'\\',
                        b'u',
                        b'0',
                        b'0',
                        HEX[(byte >> 4) as usize],
                        HEX[(byte & 15) as usize],
                    ]);
                }
                _ => self.append(&[byte]),
            }
        }
        self.append(b"\"");
    }

    fn key(&mut self, key: &[u8]) {
        self.separate();
        self.escaped(key);
        self.append(b":");
        if self.depth > 0 && self.depth as usize <= MAX_DEPTH {
            self.first[self.depth as usize - 1] = 1;
        }
    }

    fn string(&mut self, input: Option<&[u8]>) {
        self.separate();
        if let Some(input) = input {
            self.escaped(input);
        } else {
            self.append(b"null");
        }
    }

    fn integer(&mut self, value: i64) {
        let mut output = [0u8; 20];
        let mut index = output.len();
        let mut magnitude = value.unsigned_abs();
        loop {
            index -= 1;
            output[index] = b'0' + (magnitude % 10) as u8;
            magnitude /= 10;
            if magnitude == 0 {
                break;
            }
        }
        if value < 0 {
            index -= 1;
            output[index] = b'-';
        }
        self.separate();
        self.append(&output[index..]);
    }

    fn boolean(&mut self, value: c_int) {
        self.separate();
        self.append(if value == 0 { b"false" } else { b"true" });
    }

    fn null(&mut self) {
        self.separate();
        self.append(b"null");
    }

    fn raw(&mut self, input: &[u8]) {
        self.separate();
        self.append(if input.is_empty() { b"null" } else { input });
    }

    fn release(&mut self) {
        unsafe { free(self.buffer.cast()) };
        *self = Self::empty();
    }
}

// SAFETY: non-null input is readable through its NUL and disjoint from output.
unsafe fn input_bytes<'a>(input: *const c_char) -> Option<&'a [u8]> {
    if input.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(input) }.to_bytes())
    }
}

/// Initialize writable storage without reading its previous contents.
/// # Safety
/// Non-null context is aligned writable `Jsw` storage; it owns no live buffer.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_init(writer: *mut JsonWriter) {
    if !writer.is_null() {
        unsafe { writer.write(JsonWriter::empty()) };
    }
}

/// Release the native buffer and reset the context; repeated release is valid.
/// # Safety
/// Non-null context is initialized and exclusively accessible for this call.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_free(writer: *mut JsonWriter) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.release();
    }
}

/// Borrow the current output, or return null for empty/failed writers.
/// # Safety
/// Non-null context is initialized and readable. Mutation/free invalidates
/// the result; callers may wipe its bytes before freeing, as the C API allows.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_finish(writer: *const JsonWriter) -> *const c_char {
    unsafe { writer.as_ref() }
        .filter(|writer| writer.error == 0)
        .map_or(ptr::null(), |writer| writer.buffer)
}

/// Open an object (`{`) or array (`[`) with automatic separators.
/// # Safety
/// Initialized context follows `rust_core.h`; delimiter must be `{` or `[`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_open(writer: *mut JsonWriter, delimiter: u8) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.open(delimiter);
    }
}

/// Close an object (`}`) or array (`]`), preserving the legacy depth rules.
/// # Safety
/// Initialized context follows `rust_core.h`; delimiter must be `}` or `]`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_close(writer: *mut JsonWriter, delimiter: u8) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.close(delimiter);
    }
}

/// Write a key; null becomes an empty key. The next value needs no separator.
/// # Safety
/// Context and disjoint NUL-terminated input follow `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_key(writer: *mut JsonWriter, key: *const c_char) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.key(unsafe { input_bytes(key) }.unwrap_or(b""));
    }
}

/// Write escaped string bytes; null becomes a JSON null value.
/// # Safety
/// Context and disjoint NUL-terminated input follow `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_string(
    writer: *mut JsonWriter,
    input: *const c_char,
) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.string(unsafe { input_bytes(input) });
    }
}

/// Write a signed 64-bit integer without allocation or locale-sensitive format.
/// # Safety
/// Initialized context follows `rust_core.h` and is exclusively accessible.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_integer(writer: *mut JsonWriter, value: i64) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.integer(value);
    }
}

/// Write a boolean using C's established nonzero-is-true rule.
/// # Safety
/// Initialized context follows `rust_core.h` and is exclusively accessible.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_boolean(writer: *mut JsonWriter, value: c_int) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.boolean(value);
    }
}

/// Write a JSON null value.
/// # Safety
/// Initialized context follows `rust_core.h` and is exclusively accessible.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_null(writer: *mut JsonWriter) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.null();
    }
}

/// Forward raw JSON without validation; null/empty becomes JSON null.
/// # Safety
/// Context and disjoint NUL-terminated input follow `rust_core.h`.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_json_writer_raw(writer: *mut JsonWriter, input: *const c_char) {
    if let Some(writer) = unsafe { writer.as_mut() } {
        writer.raw(unsafe { input_bytes(input) }.unwrap_or(b""));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    struct OwnedWriter(JsonWriter);
    impl Drop for OwnedWriter {
        fn drop(&mut self) {
            self.0.release();
        }
    }
    impl OwnedWriter {
        fn new() -> Self {
            Self(JsonWriter::empty())
        }
        fn bytes(&self) -> &[u8] {
            unsafe { CStr::from_ptr(self.0.buffer) }.to_bytes()
        }
    }

    #[test]
    fn nested_keys_values_and_escaping() {
        let mut writer = OwnedWriter::new();
        let writer = &mut writer.0;
        writer.open(b'{');
        writer.key(b"title");
        writer.string(Some("日é😀\"\\\n\r\t\x01\x08\x0c".as_bytes()));
        writer.key(b"items");
        writer.open(b'[');
        writer.boolean(-1);
        writer.null();
        writer.raw(b"{\"unknown\":1}");
        writer.close(b']');
        writer.close(b'}');
        assert_eq!(unsafe { CStr::from_ptr(writer.buffer) }.to_bytes(), "{\"title\":\"日é😀\\\"\\\\\\n\\r\\t\\u0001\\u0008\\u000c\",\"items\":[true,null,{\"unknown\":1}]}".as_bytes());
    }

    #[test]
    fn integer_boundaries_and_null_input() {
        let mut writer = OwnedWriter::new();
        writer.0.open(b'[');
        for value in [i64::MIN, -1, 0, 1, i64::MAX] {
            writer.0.integer(value);
        }
        writer.0.string(None);
        writer.0.string(Some(b""));
        writer.0.raw(b"");
        writer.0.close(b']');
        assert_eq!(
            writer.bytes(),
            b"[-9223372036854775808,-1,0,1,9223372036854775807,null,\"\",null]"
        );
    }

    #[test]
    fn native_growth_depth_failure_and_reuse() {
        let mut writer = OwnedWriter::new();
        writer.0.string(Some(&[b'x'; 4096]));
        assert_eq!(writer.0.length, 4098);
        assert_eq!(writer.0.capacity, 8192);
        writer.0.release();
        for _ in 0..MAX_DEPTH {
            writer.0.open(b'[');
        }
        assert_eq!(writer.0.error, 0);
        writer.0.open(b'{');
        assert_eq!(writer.0.error, 1);
        assert_eq!(writer.0.depth, MAX_DEPTH as c_int);
        assert!(unsafe { nuvio_rust_json_writer_finish(&writer.0) }.is_null());
        // The C API keeps its bookkeeping after an error, but emits no more.
        let length = writer.0.length;
        writer.0.close(b']');
        writer.0.key(b"after-error");
        assert_eq!(writer.0.depth, MAX_DEPTH as c_int - 1);
        assert_eq!(writer.0.length, length);
        writer.0.release();
        writer.0.release();
        writer.0.integer(42);
        assert_eq!(writer.bytes(), b"42");
    }

    #[test]
    fn independent_threaded_writers() {
        let threads: Vec<_> = (0..4)
            .map(|id| {
                std::thread::spawn(move || {
                    for _ in 0..100 {
                        let mut writer = OwnedWriter::new();
                        writer.0.open(b'{');
                        writer.0.key(b"worker");
                        writer.0.integer(id);
                        writer.0.close(b'}');
                        assert_eq!(writer.bytes(), format!("{{\"worker\":{id}}}").as_bytes());
                    }
                })
            })
            .collect();
        for thread in threads {
            thread.join().unwrap();
        }
    }
}
