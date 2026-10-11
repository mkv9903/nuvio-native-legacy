//! Rust runtime compatibility for the target's pre-getauxval libc.
//! This is an OS interface, independent of the legacy app and its C bridge.

#[cfg(any(test, all(feature = "webos", target_arch = "arm", target_os = "linux")))]
#[derive(Debug, PartialEq, Eq)]
enum Entry {
    Value(usize),
    End,
}

#[cfg(any(test, all(feature = "webos", target_arch = "arm", target_os = "linux")))]
fn entry_value(bytes: &[u8], requested: usize) -> Option<Entry> {
    let word = std::mem::size_of::<usize>();
    for entry in bytes.chunks_exact(word * 2) {
        let kind = usize::from_ne_bytes(entry[..word].try_into().ok()?);
        let value = usize::from_ne_bytes(entry[word..].try_into().ok()?);
        if kind == 0 {
            return Some(Entry::End);
        }
        if kind == requested {
            return Some(Entry::Value(value));
        }
    }
    None
}

// Rust's startup/stack handling and ring query auxv before the UI exists.
// Use libc file operations and fixed stack storage: no Rust runtime init,
// allocations, locks, callbacks or recursion through std::fs.
#[cfg(all(feature = "webos", target_arch = "arm", target_os = "linux"))]
mod compatibility {
    use super::{entry_value, Entry};
    use std::os::raw::{c_char, c_int, c_ulong, c_void};

    extern "C" {
        fn open(path: *const c_char, flags: c_int, ...) -> c_int;
        fn read(fd: c_int, buffer: *mut c_void, size: usize) -> isize;
        fn close(fd: c_int) -> c_int;
        fn __errno_location() -> *mut c_int;
    }

    #[no_mangle]
    pub unsafe extern "C" fn getauxval(requested: c_ulong) -> c_ulong {
        let previous_error = *__errno_location();
        let fd = open(c"/proc/self/auxv".as_ptr(), 0x80000); // O_RDONLY | O_CLOEXEC
        if fd < 0 {
            return 0;
        }
        let mut buffer = [0u8; std::mem::size_of::<usize>() * 2];
        let mut filled = 0;
        loop {
            let count = read(
                fd,
                buffer[filled..].as_mut_ptr().cast(),
                buffer.len() - filled,
            );
            if count < 0 {
                if *__errno_location() == 4 {
                    continue;
                } // EINTR
                let read_error = *__errno_location();
                close(fd);
                *__errno_location() = read_error;
                return 0;
            }
            if count == 0 {
                close(fd);
                *__errno_location() = 2; // ENOENT: requested entry absent
                return 0;
            }
            filled += count as usize;
            if filled != buffer.len() {
                continue;
            }
            if let Some(entry) = entry_value(&buffer, requested as usize) {
                close(fd);
                match entry {
                    Entry::Value(value) => {
                        *__errno_location() = previous_error;
                        return value as c_ulong;
                    }
                    Entry::End => {
                        *__errno_location() = 2;
                        return 0;
                    }
                }
            }
            filled = 0;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn entry(kind: usize, value: usize) -> Vec<u8> {
        [kind.to_ne_bytes(), value.to_ne_bytes()].concat()
    }
    #[test]
    fn decodes_native_width_and_preserves_full_capability_bits() {
        let bytes = [entry(6, 4096), entry(16, usize::MAX - 1), entry(0, 0)].concat();
        assert_eq!(entry_value(&bytes, 6), Some(Entry::Value(4096)));
        assert_eq!(entry_value(&bytes, 16), Some(Entry::Value(usize::MAX - 1)));
        assert_eq!(entry_value(&bytes, 51), Some(Entry::End));
    }
    #[test]
    fn rejects_partial_entries_and_stops_at_null() {
        assert_eq!(entry_value(&[0; 3], 16), None);
        assert_eq!(
            entry_value(&[entry(0, 0), entry(16, 42)].concat(), 16),
            Some(Entry::End)
        );
        assert_eq!(entry_value(&entry(6, 4096), 16), None);
    }

    #[test]
    fn a_present_zero_value_is_distinct_from_an_absent_entry() {
        let bytes = [entry(16, 0), entry(0, 0)].concat();
        assert_eq!(entry_value(&bytes, 16), Some(Entry::Value(0)));
        assert_eq!(entry_value(&bytes, 51), Some(Entry::End));
    }
}
