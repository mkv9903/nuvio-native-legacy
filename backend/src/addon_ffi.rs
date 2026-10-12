//! Opaque addon worker and immutable results for retained native consumers.
use crate::{
    addon_backend::{AddonBackend, AddonStatus},
    addon_controller::AddonController,
    addon_snapshot::NativeAddonSnapshot,
    addon_storage::AddonStore,
    addons::{AddonUrl, InstalledAddon},
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
