//! Rust application core, called by the existing C UI through a bounded C ABI.
//!
//! URL and text helpers are migrated; the probe validates the hybrid build.

mod addon_url;
mod search_normalization;
mod stylized;
mod text_cleanup;
mod utf8;

use std::ffi::{c_char, CStr};
use std::fs::{self, OpenOptions};
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

const ABI_VERSION: u32 = 1;
const INVALID_INPUT: i32 = 1;
const THREAD_ERROR: i32 = 2;
const FILE_ERROR: i32 = 3;
const MEMORY_OK: u32 = 1;
const THREAD_OK: u32 = 2;
const FILE_OK: u32 = 4;
static SEQUENCE: AtomicUsize = AtomicUsize::new(0);

/// Mirrors `NuvioRustProbe` in `src/rust_core.h`.
#[repr(C)]
pub struct ProbeReport {
    pub abi_version: u32,
    pub checks: u32,
    pub checksum: u64,
}

/// Query this before using any other function in this ABI.
#[no_mangle]
pub extern "C" fn nuvio_rust_core_abi_version() -> u32 {
    ABI_VERSION
}

struct ProbeFile(PathBuf);

impl Drop for ProbeFile {
    fn drop(&mut self) {
        let _ = fs::remove_file(&self.0);
    }
}

fn run_probe(directory: &Path, report: &mut ProbeReport) -> Result<(), i32> {
    // Move an allocation into a bounded worker and back through its result.
    let bytes: Vec<u8> = (0..4096).map(|index| (index % 256) as u8).collect();
    report.checks |= MEMORY_OK;
    let worker = std::thread::Builder::new()
        .name("nuvio-rust-probe".into())
        .stack_size(256 * 1024)
        .spawn(move || bytes.into_iter().map(u64::from).sum::<u64>())
        .map_err(|_| THREAD_ERROR)?;
    report.checksum = worker.join().map_err(|_| THREAD_ERROR)?;
    report.checks |= THREAD_OK;

    // Never read or overwrite application data. create_new refuses collisions.
    let sequence = SEQUENCE.fetch_add(1, Ordering::Relaxed);
    let path = directory.join(format!(
        ".rust-core-probe-{}-{sequence}",
        std::process::id()
    ));
    let mut file = OpenOptions::new()
        .write(true)
        .create_new(true)
        .open(&path)
        .map_err(|_| FILE_ERROR)?;
    let cleanup = ProbeFile(path);
    let payload = format!(
        "nuvio rust core ABI={ABI_VERSION} checksum={}\n",
        report.checksum
    );
    file.write_all(payload.as_bytes()).map_err(|_| FILE_ERROR)?;
    file.sync_all().map_err(|_| FILE_ERROR)?;
    drop(file);
    let actual = fs::read(&cleanup.0).map_err(|_| FILE_ERROR)?;
    if actual != payload.as_bytes() {
        return Err(FILE_ERROR);
    }
    fs::remove_file(&cleanup.0).map_err(|_| FILE_ERROR)?;
    report.checks |= FILE_OK;
    Ok(())
}

/// Exercise allocation, a joined worker, and a private temporary-file round trip.
///
/// # Safety
/// `directory` must point to a valid NUL-terminated UTF-8 string for this call.
/// `output` must point to writable, aligned storage for one `ProbeReport`.
/// Null pointers are rejected. Rust retains neither pointer and exports no
/// allocation. Panics abort the process; they cannot unwind across this C ABI.
#[no_mangle]
pub unsafe extern "C" fn nuvio_rust_core_probe(
    directory: *const c_char,
    output: *mut ProbeReport,
) -> i32 {
    if directory.is_null() || output.is_null() {
        return INVALID_INPUT;
    }
    let directory = match unsafe { CStr::from_ptr(directory) }.to_str() {
        Ok(value) if !value.is_empty() => value,
        _ => return INVALID_INPUT,
    };
    let mut report = ProbeReport {
        abi_version: ABI_VERSION,
        checks: 0,
        checksum: 0,
    };
    let status = run_probe(Path::new(directory), &mut report)
        .err()
        .unwrap_or(0);
    unsafe { output.write(report) };
    status
}
