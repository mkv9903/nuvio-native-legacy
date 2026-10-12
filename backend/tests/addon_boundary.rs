mod support;
use nuvio_backend::{
    addon_ffi::*,
    addon_snapshot::*,
    controller::AccountController,
    ffi::{nuvio_account_destroy, nuvio_text_destroy},
    model::ProfileId,
    storage::AccountData,
};
use serde_json::json;
use std::{
    ffi::{CStr, CString},
    ptr,
    sync::{Arc, Mutex},
    time::{Duration, Instant},
};
use support::*;

unsafe fn wait_ready(handle: *const NativeAddonController) {
    let deadline = Instant::now() + Duration::from_secs(5);
    loop {
        let snapshot = unsafe { nuvio_addons_snapshot(handle) };
        let status = unsafe { (*nuvio_addons_snapshot_view(snapshot)).status };
        unsafe { nuvio_addons_snapshot_destroy(snapshot) };
        if status == 2 {
            return;
        }
        assert!(
            Instant::now() < deadline,
            "addon native action did not finish"
        );
        std::thread::sleep(Duration::from_millis(2));
    }
}

#[test]
fn native_controller_loads_toggles_installs_and_retains_full_held_views() {
    let directory = Directory::new();
    directory
        .store()
        .save(&AccountData {
            session: Some(session("member", None, "native")),
            ..Default::default()
        })
        .unwrap();
    let url = format!(
        "https://addon.invalid/{}?secret=keep/",
        "configuration".repeat(600)
    );
    let remote = Arc::new(Mutex::new(
        json!([{ "url":url, "name":"Original", "enabled":true }]),
    ));
    let state = remote.clone();
    let account = backend(
        service(move |request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(200, json!("owner")),
            "/rest/v1/rpc/sync_pull_profiles" => response(
                200,
                json!([{ "profile_index":1, "name":"Primary", "is_primary":true, "pin_enabled":false }]),
            ),
            "/rest/v1/rpc/sync_pull_profile_locks" => response(200, json!([])),
            "/rest/v1/addons" => response(200, state.lock().unwrap().clone()),
            "/rest/v1/rpc/sync_push_addons" => {
                let body: serde_json::Value = serde_json::from_slice(&request.body).unwrap();
                assert_eq!(body["p_profile_id"], 1);
                *state.lock().unwrap() = body["p_addons"].clone();
                response(200, json!({}))
            }
            _ => panic!("unexpected endpoint"),
        }),
        &directory,
    );
    account.load_profiles().unwrap();
    account
        .select_profile(ProfileId::new(1).unwrap(), None)
        .unwrap();
    let account = Box::into_raw(Box::new(AccountController::new(account).unwrap()));
    let path = CString::new(directory.path.to_str().unwrap()).unwrap();
    unsafe {
        let handle = nuvio_addons_create(account, path.as_ptr());
        assert!(!handle.is_null());
        nuvio_addons_step(handle, 0);
        wait_ready(handle);
        let held = nuvio_addons_snapshot(handle);
        let view = &*nuvio_addons_snapshot_view(held);
        assert_eq!(view.count, 1);
        assert_eq!(CStr::from_ptr((*view.addons).base).to_str().unwrap(), url);
        assert_eq!(nuvio_addons_set_enabled(handle, held, 0, 2), 0);
        assert_eq!(nuvio_addons_set_enabled(handle, held, 0, 0), 1);
        assert_eq!(nuvio_addons_set_enabled(handle, held, 0, 1), 0); // stale list
        wait_ready(handle);
        let current = nuvio_addons_snapshot(handle);
        let name = CString::new("New \" provider").unwrap();
        let new_url = CString::new("stremio://second.invalid/config/manifest.json").unwrap();
        assert_eq!(
            nuvio_addons_install(handle, current, name.as_ptr(), new_url.as_ptr()),
            1
        );
        nuvio_addons_snapshot_destroy(current);
        wait_ready(handle);
        assert_eq!(remote.lock().unwrap()[0]["enabled"], false);
        assert_eq!(
            remote.lock().unwrap()[1]["url"],
            "https://second.invalid/config"
        );
        assert_eq!(remote.lock().unwrap()[1]["name"], "New \" provider");
        let cloned = nuvio_addons_snapshot_clone(held);
        nuvio_addons_destroy(handle);
        nuvio_account_destroy(account);
        assert_eq!((*view.addons).enabled, 1);
        assert_eq!(CStr::from_ptr((*view.addons).base).to_str().unwrap(), url);
        nuvio_addons_snapshot_destroy(held);
        assert_eq!((*nuvio_addons_snapshot_view(cloned)).count, 1);
        nuvio_addons_snapshot_destroy(cloned);
    }
}

#[test]
fn null_native_handles_and_invalid_inputs_are_rejected_without_work() {
    unsafe {
        assert!(nuvio_addons_create(ptr::null(), ptr::null()).is_null());
        assert!(nuvio_addons_snapshot(ptr::null()).is_null());
        assert_eq!(nuvio_addons_revision(ptr::null()), 0);
        assert_eq!(
            nuvio_addons_set_enabled(ptr::null(), ptr::null(), usize::MAX, 1),
            0
        );
        assert_eq!(
            nuvio_addons_install(ptr::null(), ptr::null(), ptr::null(), ptr::null()),
            0
        );
        nuvio_addons_step(ptr::null(), 0);
        nuvio_addons_refresh(ptr::null());
        nuvio_addons_logout(ptr::null());
        nuvio_addons_destroy(ptr::null_mut());
        nuvio_text_destroy(ptr::null_mut());
    }
}
