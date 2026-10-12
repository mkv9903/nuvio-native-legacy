mod support;
use nuvio_backend::{controller::AccountController, ffi::*, model::Secret};
use std::{
    ffi::{c_char, CStr, CString},
    ptr, thread,
    time::{Duration, Instant},
};
use support::*;

#[test]
fn owned_snapshots_survive_actions_logout_and_controller_release() {
    let directory = Directory::new();
    let backend = backend(
        service(|_| response(200, auth("user", None, "new"))),
        &directory,
    );
    let generation = backend.begin_email();
    backend
        .finish_email(generation, Secret::new("a@b.test"), Secret::new("password"))
        .unwrap();
    let handle = Box::into_raw(Box::new(AccountController::new(backend).unwrap()));
    unsafe {
        let snapshot = nuvio_account_snapshot(handle);
        let view = nuvio_account_snapshot_view(snapshot);
        assert_eq!((*view).login, 4);
        assert_eq!(CStr::from_ptr((*view).user).to_str().unwrap(), "user");
        nuvio_account_logout(handle);
        nuvio_account_destroy(handle);
        assert_eq!((*view).login, 4);
        assert_eq!(CStr::from_ptr((*view).user).to_str().unwrap(), "user");
        nuvio_account_snapshot_destroy(snapshot);
    }
}

#[test]
fn null_handles_and_small_token_buffers_fail_without_partial_secrets() {
    let directory = Directory::new();
    let backend = backend(
        service(|_| response(200, auth("user", None, "new"))),
        &directory,
    );
    let generation = backend.begin_email();
    backend
        .finish_email(generation, Secret::new("a@b.test"), Secret::new("password"))
        .unwrap();
    let handle = Box::into_raw(Box::new(AccountController::new(backend).unwrap()));
    unsafe {
        assert_eq!(
            nuvio_account_configuration_matches(ptr::null(), ptr::null(), ptr::null()),
            0
        );
        assert!(nuvio_account_snapshot(ptr::null()).is_null());
        assert!(nuvio_account_snapshot_view(ptr::null()).is_null());
        nuvio_account_snapshot_destroy(ptr::null_mut());
        let mut tiny = [42 as c_char; 2];
        assert_eq!(
            nuvio_account_copy_token(handle, tiny.as_mut_ptr(), tiny.len()),
            0
        );
        assert_eq!(tiny[0], 0);
        let mut full = [0 as c_char; 4096];
        assert_eq!(
            nuvio_account_copy_token(handle, full.as_mut_ptr(), full.len()),
            1
        );
        assert_eq!(
            CStr::from_ptr(full.as_ptr()).to_str().unwrap(),
            token("user", None, "new")
        );
        nuvio_account_destroy(handle);
    }
}

#[test]
fn configuration_diagnostics_detect_mismatched_sources_without_exposing_values() {
    use nuvio_backend::config::{built, AccountConfig};
    let configured = i32::from(AccountConfig::built().is_ok());
    let base = CString::new(built::NUVIO_SUPABASE_URL).unwrap();
    let key = CString::new(built::NUVIO_SUPABASE_ANON_KEY).unwrap();
    let login = CString::new(built::TV_LOGIN_WEB_BASE_URL).unwrap();
    let different = CString::new(format!("mismatch-{}", built::NUVIO_SUPABASE_ANON_KEY)).unwrap();
    unsafe {
        assert_eq!(
            nuvio_account_configuration_matches(base.as_ptr(), key.as_ptr(), login.as_ptr()),
            configured
        );
        assert_eq!(
            nuvio_account_configuration_matches(base.as_ptr(), different.as_ptr(), login.as_ptr()),
            0
        );
    }
}

#[test]
fn worker_login_actions_are_nonblocking_and_publish_complete_snapshots() {
    let directory = Directory::new();
    let backend = backend(
        service(|_| response(200, auth("user", None, "new"))),
        &directory,
    );
    let handle = Box::into_raw(Box::new(AccountController::new(backend).unwrap()));
    let email = CString::new("a@b.test").unwrap();
    let password = CString::new("password").unwrap();
    unsafe {
        nuvio_account_login_email(handle, email.as_ptr(), password.as_ptr());
    }
    // The boundary copied inputs, so their native ownership ends immediately.
    drop(email);
    drop(password);
    let deadline = Instant::now() + Duration::from_secs(3);
    loop {
        let done = unsafe {
            nuvio_account_step(handle);
            let snapshot = nuvio_account_snapshot(handle);
            let view = nuvio_account_snapshot_view(snapshot);
            let done = (*view).login == 4;
            if done {
                assert_eq!(CStr::from_ptr((*view).user).to_str().unwrap(), "user");
            }
            nuvio_account_snapshot_destroy(snapshot);
            done
        };
        if done {
            break;
        }
        assert!(Instant::now() < deadline);
        thread::yield_now();
    }
    unsafe {
        nuvio_account_destroy(handle);
    }
}

#[test]
fn snapshot_text_owns_unbounded_profile_names_and_sanitizes_nul_for_c() {
    let directory = Directory::new();
    let name = "Long profile name ".repeat(100) + "\0end";
    let expected = name.replace('\0', " ");
    let backend = backend(
        service(move |request| match request.url.path() {
            "/auth/v1/token" => response(200, auth("user", None, "new")),
            "/rest/v1/rpc/get_sync_owner" => response(200, serde_json::json!("owner")),
            "/rest/v1/rpc/sync_pull_profiles" => {
                response(200, serde_json::json!([{"profile_index":1,"name":name}]))
            }
            "/rest/v1/rpc/sync_pull_profile_locks" => response(
                200,
                serde_json::json!([{"profile_id":1,"pin_enabled":false}]),
            ),
            _ => panic!("unexpected request"),
        }),
        &directory,
    );
    let generation = backend.begin_email();
    backend
        .finish_email(generation, Secret::new("a@b.test"), Secret::new("password"))
        .unwrap();
    backend.load_profiles().unwrap();
    let snapshot = NativeSnapshot::new(backend.snapshot());
    unsafe {
        let view = nuvio_account_snapshot_view(&snapshot);
        assert_eq!((*view).profile_count, 1);
        assert_eq!(
            CStr::from_ptr((*(*view).profiles).name).to_str().unwrap(),
            expected
        );
    }
}
