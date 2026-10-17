//! Owned immutable native projections of the Rust recent-search capability.
use crate::{
    controller::AccountController, search_history::SearchHistoryStore,
    search_history_runtime::SearchHistoryRuntime,
};
use std::{
    ffi::{c_char, CStr, CString},
    ptr,
    sync::Arc,
};
pub struct HistoryView {
    revision: u64,
    queries: Vec<CString>,
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_create(
    account: *const AccountController,
    directory: *const c_char,
) -> *mut SearchHistoryRuntime {
    let (Some(account), false) = (unsafe { account.as_ref() }, directory.is_null()) else {
        return ptr::null_mut();
    };
    let Ok(directory) = (unsafe { CStr::from_ptr(directory) }).to_str() else {
        return ptr::null_mut();
    };
    let result = SearchHistoryStore::open(directory).and_then(|store| {
        SearchHistoryRuntime::new(Arc::new(account.backend.clone()), Arc::new(store))
    });
    result
        .map(|runtime| Box::into_raw(Box::new(runtime)))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_destroy(handle: *mut SearchHistoryRuntime) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_step(handle: *const SearchHistoryRuntime) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.step();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_snapshot(
    handle: *const SearchHistoryRuntime,
) -> *mut HistoryView {
    let Some(handle) = (unsafe { handle.as_ref() }) else {
        return ptr::null_mut();
    };
    let snapshot = handle.snapshot();
    let queries = snapshot
        .history
        .queries()
        .iter()
        .map(|query| CString::new(query.replace('\0', " ")).expect("native query"))
        .collect();
    Box::into_raw(Box::new(HistoryView {
        revision: snapshot.revision,
        queries,
    }))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_view_destroy(view: *mut HistoryView) {
    if !view.is_null() {
        drop(unsafe { Box::from_raw(view) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_view_revision(view: *const HistoryView) -> u64 {
    unsafe { view.as_ref() }.map_or(0, |view| view.revision)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_revision(handle: *const SearchHistoryRuntime) -> u64 {
    unsafe { handle.as_ref() }.map_or(0, |handle| handle.revision())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_view_count(view: *const HistoryView) -> usize {
    unsafe { view.as_ref() }.map_or(0, |view| view.queries.len())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_view_query(
    view: *const HistoryView,
    index: usize,
) -> *const c_char {
    unsafe { view.as_ref() }
        .and_then(|view| view.queries.get(index))
        .map_or(c"".as_ptr(), |query| query.as_ptr())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_record(
    handle: *const SearchHistoryRuntime,
    query: *const c_char,
) -> i32 {
    let Some(handle) = (unsafe { handle.as_ref() }) else {
        return 0;
    };
    if query.is_null() {
        return 0;
    }
    let query = crate::search_text::decode_input(unsafe { CStr::from_ptr(query) }.to_bytes());
    i32::from(handle.record(&query))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_remove(
    handle: *const SearchHistoryRuntime,
    index: usize,
) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.remove(index);
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_clear(handle: *const SearchHistoryRuntime) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.clear_current();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_search_history_logout(handle: *const SearchHistoryRuntime) {
    if let Some(handle) = unsafe { handle.as_ref() } {
        handle.logout();
    }
}

// The retained C visual fixtures use real Rust storage with a disposable account.
// This constructor is excluded from application builds.
#[cfg(feature = "native-ui-tests")]
#[no_mangle]
pub unsafe extern "C" fn nuvio_test_search_history_create(
    directory: *const c_char,
) -> *mut SearchHistoryRuntime {
    use crate::{
        content_context::ContentScope, model::ProfileId, search_history_runtime::HistoryAccount,
    };
    struct Fixture;
    impl HistoryAccount for Fixture {
        fn history_scope(&self) -> Option<ContentScope> {
            Some(ContentScope {
                member: "visual-test-member".into(),
                owner: "visual-test-owner".into(),
                profile: ProfileId::new(1).unwrap(),
            })
        }
    }
    if directory.is_null() {
        return ptr::null_mut();
    }
    let Ok(directory) = (unsafe { CStr::from_ptr(directory) }).to_str() else {
        return ptr::null_mut();
    };
    SearchHistoryStore::open(directory)
        .and_then(|store| SearchHistoryRuntime::new(Arc::new(Fixture), Arc::new(store)))
        .map(|runtime| Box::into_raw(Box::new(runtime)))
        .unwrap_or(ptr::null_mut())
}
