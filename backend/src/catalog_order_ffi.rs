//! Transitional native access to the Rust Home order capability.
//! C renderer callers keep their historical function names in a thin adapter.
use crate::{
    catalog_order::{HomeOrderState, HomeOrderStore},
    content_context::ContentContext,
    controller::AccountController,
    error::{Error, Result},
    home_order_backend::HomeOrderBackend,
    home_order_controller::HomeOrderController,
};
use std::{
    ffi::{c_char, CStr, CString},
    ptr, slice,
    sync::{Arc, Mutex, OnceLock},
};

fn state() -> &'static HomeOrderState {
    static STATE: OnceLock<HomeOrderState> = OnceLock::new();
    STATE.get_or_init(HomeOrderState::default)
}

pub struct NativeHomeOrderController {
    pub controller: HomeOrderController,
    published: Mutex<Option<(Option<ContentContext>, u64)>>,
}
impl NativeHomeOrderController {
    pub fn new(controller: HomeOrderController) -> Self {
        Self {
            controller,
            published: Mutex::new(None),
        }
    }
    fn publish(&self) {
        let snapshot = self.controller.backend.snapshot();
        if !self.controller.backend.snapshot_current(&snapshot) {
            return;
        }
        let mut published = self.published.lock().unwrap();
        if published
            .as_ref()
            .is_some_and(|(_, revision)| *revision == snapshot.revision)
        {
            return;
        }
        let context_changed = published
            .as_ref()
            .is_none_or(|(context, _)| context != &snapshot.context);
        if state().replace(snapshot.order.clone(), context_changed) {
            println!(
                "[catalogs] Rust Home order: profile={} rows={} hidden={}",
                snapshot
                    .context
                    .as_ref()
                    .map(|context| context.profile.get())
                    .unwrap_or(0),
                snapshot.order.keys.len(),
                snapshot.order.hidden.len()
            );
        }
        if let Some(error) = snapshot.error.as_ref().or(snapshot.storage_error.as_ref()) {
            println!("[catalogs] Home order retry: {error}");
        }
        *published = Some((snapshot.context, snapshot.revision));
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_create(
    account: *const AccountController,
    directory: *const c_char,
) -> *mut NativeHomeOrderController {
    let result = (|| {
        let account = unsafe { account.as_ref() }.ok_or(Error::Configuration)?;
        let store = HomeOrderStore::open(unsafe { text(directory)? })?;
        let backend = HomeOrderBackend::new(Arc::new(account.backend.clone()), Arc::new(store));
        Ok::<_, Error>(NativeHomeOrderController::new(HomeOrderController::new(
            backend,
        )?))
    })();
    result
        .map(|controller| Box::into_raw(Box::new(controller)))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_destroy(handle: *mut NativeHomeOrderController) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_step(
    handle: *const NativeHomeOrderController,
    allow_periodic: i32,
) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.step(allow_periodic != 0);
        handle.publish();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_refresh(handle: *const NativeHomeOrderController) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.refresh();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_logout(handle: *const NativeHomeOrderController) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.controller.logout();
        handle.publish();
    } else {
        state().reset();
    }
}

pub struct NativeOrderSnapshot {
    revision: u64,
    keys: Arc<[CString]>,
}

#[no_mangle]
pub extern "C" fn nuvio_home_order_snapshot() -> *mut NativeOrderSnapshot {
    // Build the native text projection once per revision, shared across readers.
    static PROJECTION: OnceLock<Mutex<Option<(u64, Arc<[CString]>)>>> = OnceLock::new();
    let mut projection = PROJECTION.get_or_init(|| Mutex::new(None)).lock().unwrap();
    let (revision, order) = state().snapshot();
    if projection
        .as_ref()
        .is_none_or(|(current, _)| *current != revision)
    {
        let keys: Arc<[CString]> = order
            .keys
            .iter()
            .map(|key| CString::new(key.replace('\0', " ")).expect("native Home key"))
            .collect::<Vec<_>>()
            .into();
        *projection = Some((revision, keys));
    }
    let keys = projection.as_ref().unwrap().1.clone();
    Box::into_raw(Box::new(NativeOrderSnapshot { revision, keys }))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_snapshot_revision(
    snapshot: *const NativeOrderSnapshot,
) -> u64 {
    unsafe { snapshot.as_ref() }
        .map(|snapshot| snapshot.revision)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_snapshot_key(
    snapshot: *const NativeOrderSnapshot,
    index: usize,
) -> *const c_char {
    unsafe { snapshot.as_ref() }
        .and_then(|snapshot| snapshot.keys.get(index))
        .map(|key| key.as_ptr())
        .unwrap_or(c"".as_ptr())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_snapshot_destroy(snapshot: *mut NativeOrderSnapshot) {
    if !snapshot.is_null() {
        drop(unsafe { Box::from_raw(snapshot) });
    }
}
unsafe fn text<'a>(value: *const c_char) -> Result<&'a str> {
    if value.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_receive(body: *const c_char) -> i32 {
    unsafe { text(body) }
        .and_then(|body| state().decode_and_apply(body.as_bytes()))
        .map(i32::from)
        .unwrap_or(0)
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_revision() -> u64 {
    state().snapshot().0
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_count() -> usize {
    state().snapshot().1.keys.len()
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_configured() -> i32 {
    let order = state().snapshot().1;
    i32::from(!order.keys.is_empty() || !order.hidden.is_empty())
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_key(index: usize) -> *mut c_char {
    CString::new(
        state()
            .snapshot()
            .1
            .keys
            .get(index)
            .map(String::as_str)
            .unwrap_or(""),
    )
    .expect("validated Home key")
    .into_raw()
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_hidden(
    key: *const c_char,
    provider_key: *const c_char,
) -> i32 {
    i32::from(state().snapshot().1.hides(
        unsafe { text(key) }.unwrap_or(""),
        unsafe { text(provider_key) }.unwrap_or(""),
    ))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_union(
    local: *const *const c_char,
    count: usize,
    output: *mut i32,
    capacity: usize,
) -> usize {
    if local.is_null()
        || output.is_null()
        || count == 0
        || capacity == 0
        || count > i32::MAX as usize
    {
        return 0;
    }
    let local: Vec<_> = unsafe { slice::from_raw_parts(local, count) }
        .iter()
        .map(|key| unsafe { text(*key) }.unwrap_or(""))
        .collect();
    let indexes = state().snapshot().1.union(&local);
    let length = indexes.len().min(capacity);
    for (target, index) in unsafe { slice::from_raw_parts_mut(output, length) }
        .iter_mut()
        .zip(indexes)
    {
        *target = index as i32;
    }
    length
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_preference(underline: i32) -> i32 {
    let order = state().snapshot().1;
    (if underline != 0 {
        order.hide_underline
    } else {
        order.hide_unreleased
    })
    .map(i32::from)
    .unwrap_or(-1)
}
#[no_mangle]
pub extern "C" fn nuvio_home_order_reset() {
    state().reset();
}
