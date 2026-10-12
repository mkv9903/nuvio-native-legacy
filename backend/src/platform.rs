//! Compatibility with the webOS SDK's glibc 2.12. Crypto reads CPU capabilities
//! through getauxval, which glibc itself introduced after that SDK version.

#[cfg(any(all(feature = "webos", target_arch = "arm"), test))]
fn auxiliary_value(key: libc::c_ulong) -> libc::c_ulong {
    // Use allocation-free libc calls: this function may run while platform
    // libraries initialize. Keep the caller's errno when a key is present.
    unsafe {
        let previous = *libc::__errno_location();
        let descriptor = libc::open(
            b"/proc/self/auxv\0".as_ptr().cast(),
            libc::O_RDONLY | libc::O_CLOEXEC,
        );
        if descriptor < 0 {
            return 0;
        }
        let mut entry = [0 as libc::c_ulong; 2];
        let mut found = None;
        loop {
            let mut offset = 0;
            while offset < std::mem::size_of_val(&entry) {
                let read = libc::read(
                    descriptor,
                    entry.as_mut_ptr().cast::<u8>().add(offset).cast(),
                    std::mem::size_of_val(&entry) - offset,
                );
                if read < 0 && *libc::__errno_location() == libc::EINTR {
                    continue;
                }
                if read <= 0 {
                    break;
                }
                offset += read as usize;
            }
            if offset != std::mem::size_of_val(&entry) || entry[0] == 0 {
                break;
            }
            if entry[0] == key {
                found = Some(entry[1]);
                break;
            }
        }
        libc::close(descriptor);
        *libc::__errno_location() = if found.is_some() {
            previous
        } else {
            libc::ENOENT
        };
        found.unwrap_or(0)
    }
}

#[cfg(all(feature = "webos", target_arch = "arm"))]
#[no_mangle]
pub extern "C" fn getauxval(key: libc::c_ulong) -> libc::c_ulong {
    auxiliary_value(key)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn auxiliary_values_preserve_errno_on_success_and_report_absent_keys() {
        unsafe {
            *libc::__errno_location() = libc::EBUSY;
        }
        assert_eq!(
            auxiliary_value(libc::AT_PAGESZ),
            unsafe { libc::sysconf(libc::_SC_PAGESIZE) } as libc::c_ulong
        );
        assert_eq!(unsafe { *libc::__errno_location() }, libc::EBUSY);
        assert_eq!(auxiliary_value(libc::c_ulong::MAX), 0);
        assert_eq!(unsafe { *libc::__errno_location() }, libc::ENOENT);
    }
}
