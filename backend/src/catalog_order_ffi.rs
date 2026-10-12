//! Transitional native access to the Rust Home order capability.
//! C renderer callers keep their historical function names in a thin adapter.
use crate::{
    catalog_order::{HomeOrder, HomeOrderState, HomeOrderStore},
    error::{Error, Result},
    model::ProfileId,
};
use std::{
    ffi::{c_char, CStr, CString},
    slice,
    sync::{Arc, Mutex, OnceLock},
};

fn state() -> &'static HomeOrderState {
    static STATE: OnceLock<HomeOrderState> = OnceLock::new();
    STATE.get_or_init(HomeOrderState::default)
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
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_cache_load(
    directory: *const c_char,
    account: *const c_char,
    profile: u32,
) -> i32 {
    state().reset();
    let result: Result<bool> = (|| {
        let store = HomeOrderStore::open(unsafe { text(directory)? })?;
        let account = unsafe { text(account)? };
        if account.is_empty() {
            return Err(Error::InvalidData);
        }
        let order = store.load(account, ProfileId::new(profile).ok_or(Error::InvalidData)?)?;
        Ok(order.is_some_and(|order| state().apply(order)))
    })();
    result.map(i32::from).unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_cache_save(
    directory: *const c_char,
    account: *const c_char,
    profile: u32,
    body: *const c_char,
) -> i32 {
    let result: Result<bool> = (|| {
        // Missing or invalid configuration never overwrites a valid cache.
        let Some(order) = HomeOrder::decode(unsafe { text(body)? }.as_bytes())? else {
            return Ok(false);
        };
        let store = HomeOrderStore::open(unsafe { text(directory)? })?;
        store.save(
            unsafe { text(account)? },
            ProfileId::new(profile).ok_or(Error::InvalidData)?,
            &order,
        )?;
        Ok(true)
    })();
    result.map(i32::from).unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_order_cache_clear(directory: *const c_char) {
    if let Ok(store) = unsafe { text(directory) }.and_then(HomeOrderStore::open) {
        let _ = store.clear();
    }
}
