mod support;
use nuvio_backend::{
    catalog_order::{HomeOrder, HomeOrderState, HomeOrderStore},
    error::Error,
    model::ProfileId,
};
use serde_json::json;
use std::{fs, sync::Arc, thread};
use support::Directory;

fn decode(value: serde_json::Value) -> HomeOrder {
    HomeOrder::decode(&serde_json::to_vec(&value).unwrap())
        .unwrap()
        .unwrap()
}
fn profile(id: u32) -> ProfileId {
    ProfileId::new(id).unwrap()
}

#[test]
fn modern_order_is_stable_and_unions_unknown_local_rows() {
    let order = decode(json!([{"settings_json":{"items":[
        {"addon_id":"cinemeta","type":"SERIES","catalog_id":"trending","order":1},
        {"addon_id":"xperience","type":"movie","catalog_id":"foryou","order":0},
        {"addon_id":"unknown","type":"movie","catalog_id":"missing","order":2}
    ]}}]));
    assert_eq!(
        &order.keys[..2],
        ["xperience_movie_foryou", "cinemeta_series_trending"]
    );
    let local = [
        "xperience_movie_foryou",
        "cinemeta_movie_top",
        "cinemeta_series_trending",
    ];
    assert_eq!(order.union(&local), [0, 2, 1]);
    assert_eq!(order.union(&["cinemeta_series_trending"]), [0]);
    assert!(order.union(&[]).is_empty());
    let equal = decode(
        json!({"items":[{"addonId":"a","type":"Movie","catalogId":"b","order":0},{"addonId":"c","type":"movie","catalogId":"d","order":0}]}),
    );
    assert_eq!(equal.keys, ["a_movie_b", "c_movie_d"]);
}

#[test]
fn enabled_defaults_true_and_collections_have_complete_identities() {
    let order = decode(json!({"items":[
        {"addonId":"a","type":"movie","catalogId":"on"},
        {"addon_id":"a","type":"movie","catalog_id":"off","enabled":false},
        {"isCollection":true,"collectionId":"Awards"},
        {"is_collection":true}, {"addonId":"incomplete","type":"movie"}
    ]}));
    assert_eq!(
        order.keys,
        ["a_movie_on", "a_movie_off", "collection_Awards"]
    );
    assert!(!order.hides("a_movie_on", ""));
    assert!(order.hides("a_movie_off", ""));
}

#[test]
fn legacy_aliases_and_long_hidden_keys_keep_complete_unicode_and_escapes() {
    let provider_key = format!(
        "https://addon.invalid/{}_movie_top_Example",
        "configuration".repeat(400)
    );
    for name in [
        "catalog_order_keys",
        "home_catalog_order",
        "catalog_order",
        "order",
    ] {
        let order = decode(
            json!({name:["first","Unicode 日 \"quote\"","first"], "hidden_catalog_keys":[provider_key]}),
        );
        assert_eq!(order.keys, ["first", "Unicode 日 \"quote\""]);
        assert!(order.hides("unknown", &provider_key));
        assert!(!format!("{order:?}").contains("configuration"));
        assert_eq!(order.union(&["first", "first", "unknown"]), [0, 1, 2]);
    }
    for name in [
        "disabled_catalog_keys",
        "hidden_catalog_keys",
        "catalog_disabled_keys",
        "home_catalog_disabled",
        "disabled",
    ] {
        assert!(decode(json!({name:["hidden"]})).hides("hidden", ""));
    }
    assert_eq!(
        decode(json!({"order":[], "home_catalog_order":["first"], "catalog_order_keys":[]}))
            .keys
            .len(),
        0
    );
}

#[test]
fn missing_configuration_preserves_state_but_explicit_empty_clears_it() {
    let state = HomeOrderState::default();
    let body = serde_json::to_vec(
        &json!({"items":[{"addonId":"a","type":"movie","catalogId":"b","enabled":false}]}),
    )
    .unwrap();
    assert!(state.decode_and_apply(&body).unwrap());
    let revision = state.snapshot().0;
    assert!(!state.decode_and_apply(&body).unwrap());
    for missing in [
        "[]",
        "{}",
        "[{\"settings_json\":{}}]",
        "{\"settings_json\":null}",
        "{\"items\":[{}]}",
    ] {
        assert!(!state.decode_and_apply(missing.as_bytes()).unwrap());
        assert_eq!(state.snapshot().0, revision);
        assert!(state.snapshot().1.hides("a_movie_b", ""));
    }
    assert!(state.decode_and_apply(br#"{"items":[]}"#).unwrap());
    assert!(!state.decode_and_apply(br#"{"items":[]}"#).unwrap());
    assert!(state.snapshot().1.hidden.is_empty());
    state.decode_and_apply(&body).unwrap();
    assert!(state
        .decode_and_apply(br#"{"catalog_order_keys":[],"disabled_catalog_keys":[]}"#)
        .unwrap());
    assert_eq!(state.snapshot().1.union(&["local", "new"]), [0, 1]);
}

#[test]
fn absent_boolean_preferences_preserve_accepted_values_within_the_profile() {
    let state = HomeOrderState::default();
    assert!(state.snapshot().1.hide_unreleased.is_none());
    state
        .decode_and_apply(
            br#"{"order":["a"],"hide_unreleased_content":true,"hide_catalog_underline":false}"#,
        )
        .unwrap();
    assert_eq!(state.snapshot().1.hide_unreleased, Some(true));
    assert_eq!(state.snapshot().1.hide_underline, Some(false));
    state.decode_and_apply(br#"{"order":["b"]}"#).unwrap();
    assert_eq!(state.snapshot().1.hide_unreleased, Some(true));
    state.reset();
    assert_eq!(state.snapshot().1.hide_unreleased, None);
    assert!(state.snapshot().1.keys.is_empty());
}

#[test]
fn accounts_beyond_the_old_limit_keep_late_order_and_hidden_entries() {
    let items: Vec<_> = (0..4096).map(|index| json!({"addon_id":"a","type":"movie","catalog_id":format!("c{index}"),"order":4095-index,"enabled":index!=4095})).collect();
    let order = decode(json!({"items":items}));
    assert_eq!(order.keys.len(), 4096);
    assert_eq!(order.keys[0], "a_movie_c4095");
    assert!(order.hides("a_movie_c4095", ""));
    let local: Vec<_> = (0..7000).map(|index| format!("a_movie_c{index}")).collect();
    let borrowed: Vec<_> = local.iter().map(String::as_str).collect();
    let indexes = order.union(&borrowed);
    assert_eq!(indexes.len(), 7000);
    assert_eq!(&indexes[..2], [4095, 4094]);
    assert_eq!(indexes[4096], 4096);
    assert_eq!(indexes[6999], 6999);
}

#[test]
fn malformed_configuration_cannot_replace_a_valid_snapshot() {
    let state = HomeOrderState::default();
    state.decode_and_apply(br#"{"order":["valid"]}"#).unwrap();
    let original = state.snapshot();
    for body in [
        "",
        "{",
        "null",
        "[{},{}]",
        "[false]",
        "{\"items\":false}",
        "{\"order\":[1]}",
        "{\"settings_json\":\"serialized\"}",
        "{\"items\":[],\"hide_unreleased_content\":\"true\"}",
        "{\"order\":[\"a\\u0000b\"]}",
    ] {
        assert!(state.decode_and_apply(body.as_bytes()).is_err(), "{body}");
        assert_eq!(state.snapshot().0, original.0);
        assert_eq!(state.snapshot().1, original.1);
    }
    assert_eq!(
        HomeOrder::decode(&vec![b' '; 4 * 1024 * 1024 + 1]),
        Err(Error::ResponseLimit)
    );
}

#[test]
fn held_snapshots_survive_concurrent_replacement_and_reset() {
    let state = Arc::new(HomeOrderState::default());
    state.apply(decode(json!({"order":["original"]})));
    let original = state.snapshot().1;
    let reader = state.clone();
    let worker = thread::spawn(move || {
        for _ in 0..10000 {
            let snapshot = reader.snapshot().1;
            assert!(
                snapshot.keys == ["original"]
                    || snapshot.keys == ["aaa_1", "aaa_2"]
                    || snapshot.keys == ["bbb_1"]
            );
        }
    });
    for index in 0..10000 {
        state.apply(decode(if index % 2 == 0 {
            json!({"order":["aaa_1","aaa_2"]})
        } else {
            json!({"order":["bbb_1"]})
        }));
    }
    worker.join().unwrap();
    state.reset();
    assert_eq!(original.keys, ["original"]);
    assert!(state.snapshot().1.keys.is_empty());
}

#[test]
fn versioned_cache_survives_restart_and_isolates_accounts_and_profiles() {
    let directory = Directory::new();
    let store = HomeOrderStore::open(&directory.path).unwrap();
    let order = decode(json!({"order":["first","second"],"hidden_catalog_keys":["second"]}));
    store.save("account-a", profile(3), &order).unwrap();
    drop(store);
    let reopened = HomeOrderStore::open(&directory.path).unwrap();
    assert_eq!(reopened.load("account-a", profile(3)).unwrap(), Some(order));
    assert!(reopened.load("account-b", profile(3)).unwrap().is_none());
    assert!(reopened.load("account-a", profile(4)).unwrap().is_none());
    reopened
        .save("account-a", profile(9), &HomeOrder::default())
        .unwrap();
    assert_eq!(
        reopened.load("account-a", profile(9)).unwrap(),
        Some(HomeOrder::default())
    );
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(directory.path.join("rust-backend-v1/home-order-p3.json"))
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o600
        );
    }
    fs::write(
        directory.path.join("home-catalog-settings-p3.json"),
        "old C data",
    )
    .unwrap();
    reopened.clear().unwrap();
    assert!(reopened.load("account-a", profile(3)).unwrap().is_none());
    assert!(reopened.load("account-a", profile(9)).unwrap().is_none());
    assert!(directory
        .path
        .join("home-catalog-settings-p3.json")
        .exists());
}

#[test]
fn invalid_versions_ownership_and_storage_targets_are_not_imported() {
    let directory = Directory::new();
    let store = HomeOrderStore::open(&directory.path).unwrap();
    let file = directory.path.join("rust-backend-v1/home-order-p1.json");
    for body in ["", "{}", "{\"version\":2,\"app_id\":\"space.mkv.nuvio.rusttest\",\"account\":\"a\",\"profile\":1,\"order\":{\"keys\":[],\"hidden\":[],\"hide_unreleased\":null,\"hide_underline\":null}}"] {
        fs::write(&file, body).unwrap();
        assert!(store.load("a", profile(1)).is_err());
        assert_eq!(fs::read_to_string(&file).unwrap(), body);
    }
    assert!(HomeOrderStore::open(directory.parent.join("space.mkv.nuvio.native")).is_err());
    #[cfg(unix)]
    {
        use std::os::unix::fs::symlink;
        fs::remove_file(&file).unwrap();
        let outside = directory.parent.join("private.json");
        fs::write(&outside, "outside").unwrap();
        symlink(&outside, &file).unwrap();
        assert!(store.load("a", profile(1)).is_err());
        store.save("a", profile(1), &HomeOrder::default()).unwrap();
        assert_eq!(fs::read_to_string(outside).unwrap(), "outside");
    }
}

#[test]
fn native_order_boundary_owns_long_text_and_respects_output_capacity() {
    use nuvio_backend::{catalog_order_ffi::*, ffi::nuvio_text_destroy};
    use std::ffi::{CStr, CString};
    let key = "complete-key-".repeat(500);
    let body = CString::new(json!({"order":[key,"first"],"disabled":[key]}).to_string()).unwrap();
    nuvio_home_order_reset();
    assert_eq!(unsafe { nuvio_home_order_receive(body.as_ptr()) }, 1);
    assert_eq!(nuvio_home_order_count(), 2);
    let held = nuvio_home_order_key(0);
    let snapshot = nuvio_home_order_snapshot();
    let borrowed = unsafe { nuvio_home_order_snapshot_key(snapshot, 0) };
    let first = CString::new("first").unwrap();
    let unknown = CString::new("unknown").unwrap();
    let local = [first.as_ptr(), unknown.as_ptr()];
    let mut output = [-1, -99];
    assert_eq!(
        unsafe { nuvio_home_order_union(local.as_ptr(), 2, output.as_mut_ptr(), 1) },
        1
    );
    assert_eq!(output, [0, -99]);
    assert_eq!(
        unsafe { nuvio_home_order_union(std::ptr::null(), 0, output.as_mut_ptr(), 1) },
        0
    );
    assert_eq!(unsafe { nuvio_home_order_receive(std::ptr::null()) }, 0);
    nuvio_home_order_reset();
    assert_eq!(unsafe { CStr::from_ptr(held) }.to_str().unwrap(), key);
    assert_eq!(unsafe { CStr::from_ptr(borrowed) }.to_str().unwrap(), key);
    unsafe {
        nuvio_text_destroy(held);
        nuvio_home_order_snapshot_destroy(snapshot);
    }
}
