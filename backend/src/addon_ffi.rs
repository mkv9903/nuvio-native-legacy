//! Opaque addon worker and immutable results for retained native consumers.
use crate::{
    addon_backend::{AddonBackend, AddonStatus},
    addon_controller::AddonController,
    addon_snapshot::NativeAddonSnapshot,
    addon_storage::AddonStore,
    addons::{AddonUrl, InstalledAddon, Manifest},
    controller::AccountController,
    error::{Error, Result},
};
use std::{
    ffi::{c_char, CStr},
    ptr,
    sync::{Arc, Mutex},
};

pub struct NativeAddonController {
    pub controller: AddonController,
    native: Mutex<Option<NativeAddonSnapshot>>,
}
impl NativeAddonController {
    pub fn new(controller: AddonController) -> Self {
        Self {
            controller,
            native: Mutex::new(None),
        }
    }
    pub fn snapshot(&self) -> NativeAddonSnapshot {
        let snapshot = self.controller.backend.snapshot();
        let mut cached = self
            .native
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        if cached
            .as_ref()
            .is_none_or(|cached| cached.snapshot.revision != snapshot.addons.revision)
        {
            let status = match snapshot.status {
                AddonStatus::Unavailable => 0,
                AddonStatus::Loading => 1,
                AddonStatus::Ready => 2,
                AddonStatus::RetryPending => 3,
                AddonStatus::Unsaved => 4,
                AddonStatus::RecoveryPaused => 5,
            };
            *cached = Some(NativeAddonSnapshot::with_status(
                snapshot.addons.clone(),
                status,
                &snapshot
                    .error
                    .as_ref()
                    .map(ToString::to_string)
                    .unwrap_or_default(),
            ));
        }
        cached.as_ref().expect("native addon snapshot").clone()
    }
}
unsafe fn text<'a>(input: *const c_char) -> Result<&'a str> {
    if input.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(input) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_create(
    account: *const AccountController,
    directory: *const c_char,
) -> *mut NativeAddonController {
    let result = (|| {
        let account = unsafe { account.as_ref() }.ok_or(Error::Configuration)?;
        let store = AddonStore::open(unsafe { text(directory)? })?;
        let backend = AddonBackend::new(Arc::new(account.backend.clone()), Arc::new(store));
        Ok::<_, Error>(NativeAddonController::new(AddonController::new(backend)?))
    })();
    result
        .map(|controller| Box::into_raw(Box::new(controller)))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_destroy(handle: *mut NativeAddonController) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_step(
    handle: *const NativeAddonController,
    force_primary: i32,
) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.step(force_primary != 0);
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_step_controlled(
    handle: *const NativeAddonController,
    force_primary: i32,
    allow_periodic: i32,
) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle
            .controller
            .step_controlled(force_primary != 0, allow_periodic != 0);
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addon_normalize_url(url: *const c_char) -> *mut c_char {
    unsafe { text(url) }
        .and_then(AddonUrl::parse)
        .ok()
        .and_then(|url| std::ffi::CString::new(url.as_str()).ok())
        .map(std::ffi::CString::into_raw)
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addon_public_host(url: *const c_char) -> *mut c_char {
    let result = (|| {
        let parsed = url::Url::parse(unsafe { text(url).ok()? }).ok()?;
        let host = parsed.host()?.to_string();
        let host = parsed
            .port()
            .map(|port| format!("{host}:{port}"))
            .unwrap_or(host);
        std::ffi::CString::new(host).ok()
    })();
    result
        .map(std::ffi::CString::into_raw)
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addon_manifest_valid(body: *const c_char) -> i32 {
    i32::from(
        unsafe { text(body) }
            .and_then(|body| Manifest::decode(body.as_bytes()))
            .is_ok(),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_refresh(handle: *const NativeAddonController) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.refresh();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_logout(handle: *const NativeAddonController) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.logout();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot(
    handle: *const NativeAddonController,
) -> *mut NativeAddonSnapshot {
    unsafe { handle.as_ref() }
        .map(|handle| Box::into_raw(Box::new(handle.snapshot())))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_revision(handle: *const NativeAddonController) -> u64 {
    unsafe { handle.as_ref() }
        .map(|handle| handle.controller.backend.snapshot().addons.revision)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_list_revision(handle: *const NativeAddonController) -> u64 {
    unsafe { handle.as_ref() }
        .map(|handle| handle.controller.backend.snapshot().addons.list_revision)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_current(
    handle: *const NativeAddonController,
    snapshot: *const NativeAddonSnapshot,
) -> i32 {
    match (unsafe { handle.as_ref() }, unsafe { snapshot.as_ref() }) {
        (Some(handle), Some(snapshot)) => i32::from(
            handle
                .controller
                .backend
                .snapshot_current(&snapshot.snapshot),
        ),
        _ => 0,
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_attach_manifest(
    handle: *const NativeAddonController,
    snapshot: *const NativeAddonSnapshot,
    index: usize,
    body: *const c_char,
) -> i32 {
    let result = (|| {
        let handle = unsafe { handle.as_ref() }.ok_or(Error::Configuration)?;
        let snapshot = unsafe { snapshot.as_ref() }.ok_or(Error::InvalidData)?;
        let manifest = Manifest::decode(unsafe { text(body)? }.as_bytes())?;
        handle
            .controller
            .backend
            .attach_snapshot_manifest(&snapshot.snapshot, index, manifest)
    })();
    i32::from(result.is_ok())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_report_response(
    handle: *const NativeAddonController,
    snapshot: *const NativeAddonSnapshot,
    index: usize,
    responded: i32,
) -> i64 {
    match (unsafe { handle.as_ref() }, unsafe { snapshot.as_ref() }) {
        (Some(handle), Some(snapshot)) if matches!(responded, 0 | 1) => handle
            .controller
            .backend
            .report_source_response(&snapshot.snapshot, index, responded != 0)
            .map(i64::from)
            .unwrap_or(-1),
        _ => -1,
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_set_enabled(
    handle: *const NativeAddonController,
    snapshot: *const NativeAddonSnapshot,
    index: usize,
    enabled: i32,
) -> i32 {
    if !matches!(enabled, 0 | 1) {
        return 0;
    }
    match (unsafe { handle.as_ref() }, unsafe { snapshot.as_ref() }) {
        (Some(handle), Some(snapshot)) => i32::from(
            handle
                .controller
                .set_enabled(&snapshot.snapshot, index, enabled != 0)
                .is_ok(),
        ),
        _ => 0,
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_install(
    handle: *const NativeAddonController,
    snapshot: *const NativeAddonSnapshot,
    name: *const c_char,
    url: *const c_char,
) -> i32 {
    let result = (|| {
        let handle = unsafe { handle.as_ref() }.ok_or(Error::Configuration)?;
        let snapshot = unsafe { snapshot.as_ref() }.ok_or(Error::InvalidData)?;
        let name = unsafe { text(name)? }.to_owned();
        let url = AddonUrl::parse(unsafe { text(url)? })?;
        handle.controller.install(
            &snapshot.snapshot,
            InstalledAddon {
                name,
                url,
                enabled: true,
            },
        )
    })();
    i32::from(result.unwrap_or(false))
}
