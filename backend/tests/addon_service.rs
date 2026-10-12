mod support;
use nuvio_backend::{
    account::AccountBackend,
    addon_service::AddonAccount,
    addons::{AddonUrl, InstalledAddon},
    error::Error,
    http::{Request, Response},
    model::{ProfileId, Secret},
    storage::AccountData,
};
use serde_json::{json, Value};
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    Arc, Barrier, Mutex,
};
use support::*;

fn profile(id: u32) -> ProfileId {
    ProfileId::new(id).unwrap()
}
fn initialize(
    directory: &Directory,
    callback: impl Fn(Request) -> nuvio_backend::error::Result<Response> + Send + Sync + 'static,
) -> AccountBackend {
    directory
        .store()
        .save(&AccountData {
            session: Some(session("member", None, "old")),
            ..Default::default()
        })
        .unwrap();
    let account = backend(
        service(move |request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(200, json!("shared-owner & extra")),
            "/rest/v1/rpc/sync_pull_profiles" => response(
                200,
                json!([
                    {"profile_index":1,"name":"Primary","is_primary":true,"pin_enabled":false},
                    {"profile_index":2,"name":"Shared","uses_primary_addons":true,"pin_enabled":false},
                    {"profile_index":3,"name":"Independent","uses_primary_addons":false,"pin_enabled":false}
                ]),
            ),
            "/rest/v1/rpc/sync_pull_profile_locks" => response(200, json!([])),
            _ => callback(request),
        }),
        directory,
    );
    account.load_profiles().unwrap();
    assert!(account.content_context(false).is_none());
    account.select_profile(profile(2), None).unwrap();
    account
}

#[test]
fn authenticated_pages_use_the_owner_and_effective_addon_profile() {
    let directory = Directory::new();
    let calls = Arc::new(AtomicUsize::new(0));
    let counter = calls.clone();
    let account = initialize(&directory, move |request| {
        assert_eq!(request.url.path(), "/rest/v1/addons");
        assert_eq!(request.authorization.expose(), token("member", None, "old"));
        let query: std::collections::HashMap<_, _> =
            request.url.query_pairs().into_owned().collect();
        assert_eq!(query["user_id"], "eq.shared-owner & extra");
        assert_eq!(query["profile_id"], "eq.1");
        assert_eq!(query["order"], "sort_order.asc");
        assert_eq!(query["limit"], "500");
        let offset = counter.fetch_add(1, Ordering::SeqCst) * 500;
        assert_eq!(query["offset"], offset.to_string());
        let rows: Vec<_> = (offset..if offset==0 {500} else {501}).map(|index| json!({"url":format!("https://addon{index}.invalid/manifest.json"),"enabled":index!=500})).collect();
        response(200, json!(rows))
    });
    let context = account.content_context(false).unwrap();
    assert_eq!(context.profile.get(), 2);
    assert_eq!(context.addons.profile.get(), 1);
    let addons = account.read_addons(&context).unwrap();
    assert_eq!(addons.len(), 501);
    assert!(!addons[500].enabled);
    assert_eq!(calls.load(Ordering::SeqCst), 2);
}

#[test]
fn push_uses_serde_and_preserves_full_urls_disabled_entries_and_order() {
    let directory = Directory::new();
    let captured = Arc::new(Mutex::new(Value::Null));
    let output = captured.clone();
    let account = initialize(&directory, move |request| {
        assert_eq!(request.url.path(), "/rest/v1/rpc/sync_push_addons");
        *output.lock().unwrap() = serde_json::from_slice(&request.body).unwrap();
        response(200, json!({}))
    });
    let context = account.content_context(false).unwrap();
    let addons = vec![
        InstalledAddon {
            name: "Quote \" and backslash \\ 日".into(),
            url: AddonUrl::parse(&format!(
                "https://addon.invalid/{}?key=keep/",
                "config".repeat(700)
            ))
            .unwrap(),
            enabled: false,
        },
        InstalledAddon {
            name: String::new(),
            url: AddonUrl::parse("https://second.invalid").unwrap(),
            enabled: true,
        },
    ];
    account.write_addons(&context, &addons).unwrap();
    let body = captured.lock().unwrap();
    assert_eq!(body["p_profile_id"], 1);
    assert_eq!(body["p_addons"][0]["url"], addons[0].url.as_str());
    assert_eq!(body["p_addons"][0]["name"], addons[0].name);
    assert_eq!(body["p_addons"][0]["enabled"], false);
    assert_eq!(body["p_addons"][1]["sort_order"], 1);
    assert!(body["p_addons"][1].get("name").is_none());
    drop(body);
    account.write_addons(&context, &[]).unwrap();
    assert_eq!(captured.lock().unwrap()["p_addons"], json!([]));
}

#[test]
fn profile_round_trip_invalidates_old_work_even_with_identical_shared_addons() {
    let directory = Directory::new();
    let started = Arc::new(Barrier::new(2));
    let release = Arc::new(Barrier::new(2));
    let ready = started.clone();
    let finish = release.clone();
    let account = initialize(&directory, move |_| {
        ready.wait();
        finish.wait();
        response(200, json!([]))
    });
    let old = account.content_context(false).unwrap();
    std::thread::scope(|scope| {
        let request = scope.spawn(|| account.read_addons(&old));
        started.wait();
        account.select_profile(profile(1), None).unwrap();
        account.select_profile(profile(2), None).unwrap();
        let new = account.content_context(false).unwrap();
        assert_eq!(old.addons, new.addons);
        assert_ne!(old.content_generation, new.content_generation);
        release.wait();
        assert_eq!(request.join().unwrap(), Err(Error::Cancelled));
    });
    assert_eq!(account.read_addons(&old), Err(Error::Cancelled));
}

#[test]
fn independent_profiles_and_forced_primary_have_separate_contexts() {
    let directory = Directory::new();
    let account = initialize(&directory, |_| response(200, json!([])));
    account.select_profile(profile(3), None).unwrap();
    let own = account.content_context(false).unwrap();
    let primary = account.content_context(true).unwrap();
    assert_eq!(own.addons.profile.get(), 3);
    assert_eq!(primary.addons.profile.get(), 1);
    assert_eq!(primary.profile.get(), 3);
    account.logout();
    assert!(account.content_context(false).is_none());
    assert!(!account.content_context_current(&own));
}

#[test]
fn failures_on_later_pages_and_malformed_lists_cannot_return_partial_data() {
    for failure in [
        Error::Network,
        Error::Tls,
        Error::Http(503),
        Error::InvalidData,
    ] {
        let directory = Directory::new();
        let expected = failure.clone();
        let calls = Arc::new(AtomicUsize::new(0));
        let counter = calls.clone();
        let account = initialize(&directory, move |_| {
            if counter.fetch_add(1, Ordering::SeqCst) == 0 {
                response(
                    200,
                    json!((0..500)
                        .map(|index| json!({"url":format!("https://a{index}.invalid")}))
                        .collect::<Vec<_>>()),
                )
            } else if failure == Error::InvalidData {
                response(200, json!([{"url":"https://valid.invalid"},{}]))
            } else {
                Err(failure.clone())
            }
        });
        assert_eq!(
            account.read_addons(&account.content_context(false).unwrap()),
            Err(expected)
        );
        assert_eq!(calls.load(Ordering::SeqCst), 2);
    }
}

#[test]
fn pending_unknown_owner_or_unverified_profile_never_has_a_content_context() {
    let directory = Directory::new();
    directory
        .store()
        .save(&AccountData {
            session: Some(session("member", None, "old")),
            ..Default::default()
        })
        .unwrap();
    let account = backend(
        service(|request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => Err(Error::Network),
            "/rest/v1/rpc/sync_pull_profiles" => {
                response(200, json!([{"profile_index":1,"name":"Unverified"}]))
            }
            _ => Err(Error::Network),
        }),
        &directory,
    );
    assert!(account.content_context(false).is_none());
    account.load_profiles().unwrap();
    assert!(account.content_context(false).is_none());
    assert!(account
        .select_profile(profile(1), Some(Secret::new("1234")))
        .is_err());
    assert!(account.content_context(false).is_none());
}
