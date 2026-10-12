mod support;
use nuvio_backend::{
    account::{AccountBackend, LoginState},
    error::Error,
    model::{ProfileId, ProfileLock, Secret},
    storage::AccountData,
};
use serde_json::json;
use std::{
    fs,
    sync::{
        atomic::{AtomicU64, AtomicUsize, Ordering},
        Arc, Barrier,
    },
    thread,
};
use support::*;

fn seed(directory: &Directory, user: &str, expiration: Option<u64>) {
    directory
        .store()
        .save(&AccountData {
            session: Some(session(user, expiration, "old")),
            ..AccountData::default()
        })
        .unwrap();
}

#[test]
fn refresh_outages_keep_memory_and_disk_and_a_later_attempt_recovers() {
    for failure in [
        Error::Network,
        Error::Tls,
        Error::Timeout,
        Error::Http(429),
        Error::Http(503),
    ] {
        let directory = Directory::new();
        seed(&directory, "user", Some(999));
        let calls = Arc::new(AtomicUsize::new(0));
        let count = calls.clone();
        let expected = failure.clone();
        let service = service(move |request| {
            if request.url.query() == Some("grant_type=refresh_token") {
                if count.fetch_add(1, Ordering::SeqCst) == 0 {
                    return Err(failure.clone());
                }
                return response(200, auth("user", Some(5000), "new"));
            }
            assert_eq!(
                request.authorization.expose(),
                token("user", Some(5000), "new")
            );
            response(200, json!("ok"))
        });
        let backend = backend(service, &directory);
        assert_eq!(backend.rpc("rename_profile", b"{}").err(), Some(expected));
        assert_eq!(backend.snapshot().login, LoginState::LoggedIn);
        assert_eq!(
            backend.user_token().unwrap().expose(),
            token("user", Some(999), "old")
        );
        assert_eq!(
            directory
                .store()
                .load()
                .unwrap()
                .session
                .unwrap()
                .refresh
                .expose(),
            "refresh-old"
        );
        assert!(backend.rpc("rename_profile", b"{}").is_ok());
        assert_eq!(calls.load(Ordering::SeqCst), 2);
        assert_eq!(
            directory
                .store()
                .load()
                .unwrap()
                .session
                .unwrap()
                .refresh
                .expose(),
            "refresh-new"
        );
    }
}

#[test]
fn authenticated_request_outages_preserve_login_but_refresh_rejection_logs_out() {
    for status in [429, 500, 503] {
        let directory = Directory::new();
        seed(&directory, "user", None);
        let backend = backend(service(move |_| response(status, json!({}))), &directory);
        assert_eq!(backend.rpc("rename_profile", b"{}").unwrap().status, status);
        assert_eq!(backend.snapshot().login, LoginState::LoggedIn);
        assert!(directory.store().load().unwrap().session.is_some());
    }
    let directory = Directory::new();
    seed(&directory, "user", Some(999));
    let backend = backend(
        service(|_| response(400, json!({"error":"invalid_grant"}))),
        &directory,
    );
    assert_eq!(
        backend.rpc("rename_profile", b"{}").err(),
        Some(Error::Http(400))
    );
    assert_eq!(backend.snapshot().login, LoginState::LoggedOut);
    assert!(backend.user_token().is_none());
    assert!(directory.store().load().unwrap().session.is_none());
}

#[test]
fn concurrent_rpc_and_table_401s_share_one_rotating_refresh() {
    const WORKERS: usize = 8;
    let directory = Directory::new();
    seed(&directory, "user", None);
    let first_requests = Arc::new(Barrier::new(WORKERS));
    let refreshes = Arc::new(AtomicUsize::new(0));
    let count = refreshes.clone();
    let gate = first_requests.clone();
    let backend = backend(
        service(move |request| {
            if request.url.query() == Some("grant_type=refresh_token") {
                let body: serde_json::Value = serde_json::from_slice(&request.body).unwrap();
                assert_eq!(body["refresh_token"], "refresh-old");
                count.fetch_add(1, Ordering::SeqCst);
                return response(200, auth("user", None, "new"));
            }
            if request.authorization.expose() == token("user", None, "old") {
                gate.wait();
                return response(401, json!({}));
            }
            assert_eq!(request.authorization.expose(), token("user", None, "new"));
            response(200, json!("ok"))
        }),
        &directory,
    );
    thread::scope(|scope| {
        let mut handles = Vec::new();
        for worker in 0..WORKERS {
            let backend = &backend;
            handles.push(scope.spawn(move || {
                if worker % 2 == 0 {
                    backend.rpc("test_rpc", b"{}")
                } else {
                    backend.table("addons", "select=*&profile_id=eq.1")
                }
            }));
        }
        for handle in handles {
            assert_eq!(handle.join().unwrap().unwrap().status, 200);
        }
    });
    assert_eq!(refreshes.load(Ordering::SeqCst), 1);
    assert_eq!(
        directory
            .store()
            .load()
            .unwrap()
            .session
            .unwrap()
            .refresh
            .expose(),
        "refresh-new"
    );
}

#[test]
fn private_token_readers_never_observe_torn_pairs_during_repeated_refresh() {
    let directory = Directory::new();
    seed(&directory, "user", Some(999));
    let counter = Arc::new(AtomicUsize::new(0));
    let calls = counter.clone();
    let backend = backend(
        service(move |request| {
            if request.url.query() == Some("grant_type=refresh_token") {
                let generation = calls.fetch_add(1, Ordering::SeqCst);
                response(200, auth("user", Some(999), &format!("gen{generation}")))
            } else {
                response(200, json!({}))
            }
        }),
        &directory,
    );
    thread::scope(|scope| {
        for _ in 0..4 {
            let backend = &backend;
            scope.spawn(move || {
                for _ in 0..10000 {
                    let token = backend.user_token().unwrap();
                    let parsed = nuvio_backend::model::Session::from_tokens(
                        token,
                        Secret::new("reader"),
                        false,
                    )
                    .unwrap();
                    assert_eq!(parsed.subject, "user");
                    assert_eq!(parsed.expires_at, Some(999));
                }
            });
        }
        for _ in 0..32 {
            backend.rpc("test_rpc", b"{}").unwrap();
        }
    });
    assert_eq!(counter.load(Ordering::SeqCst), 32);
}

#[test]
fn late_login_after_cancel_or_logout_never_restores_credentials() {
    for logout in [false, true] {
        let directory = Directory::new();
        let started = Arc::new(Barrier::new(2));
        let release = Arc::new(Barrier::new(2));
        let ready = started.clone();
        let finish = release.clone();
        let backend = backend(
            service(move |_| {
                ready.wait();
                finish.wait();
                response(200, auth("user", None, "late"))
            }),
            &directory,
        );
        let generation = backend.begin_email();
        thread::scope(|scope| {
            let handle = scope.spawn(|| {
                backend.finish_email(generation, Secret::new("a@b.test"), Secret::new("password"))
            });
            started.wait();
            if logout {
                backend.logout();
            } else {
                backend.cancel();
            }
            release.wait();
            assert_eq!(handle.join().unwrap(), Err(Error::Cancelled));
        });
        assert_eq!(backend.snapshot().login, LoginState::LoggedOut);
        assert!(directory.store().load().unwrap().session.is_none());
    }
}

#[test]
fn stale_rejected_refresh_cannot_log_out_a_new_account() {
    let directory = Directory::new();
    seed(&directory, "old-user", Some(999));
    let started = Arc::new(Barrier::new(2));
    let release = Arc::new(Barrier::new(2));
    let ready = started.clone();
    let finish = release.clone();
    let backend = backend(
        service(move |request| {
            if request.url.query() == Some("grant_type=refresh_token") {
                ready.wait();
                finish.wait();
                response(400, json!({}))
            } else {
                response(200, auth("new-user", None, "new"))
            }
        }),
        &directory,
    );
    thread::scope(|scope| {
        let handle = scope.spawn(|| backend.rpc("test_rpc", b"{}"));
        started.wait();
        let generation = backend.begin_email();
        backend
            .finish_email(
                generation,
                Secret::new("new@b.test"),
                Secret::new("password"),
            )
            .unwrap();
        release.wait();
        assert_eq!(handle.join().unwrap().err(), Some(Error::Cancelled));
    });
    assert_eq!(backend.snapshot().user, "new-user");
    assert_eq!(backend.snapshot().login, LoginState::LoggedIn);
    assert_eq!(
        directory.store().load().unwrap().session.unwrap().subject,
        "new-user"
    );
}

#[test]
fn storage_failure_keeps_a_successful_login_usable_in_memory() {
    let directory = Directory::new();
    let backend = backend(
        service(|_| response(200, auth("user", None, "new"))),
        &directory,
    );
    fs::create_dir(directory.account_file()).unwrap();
    let generation = backend.begin_email();
    backend
        .finish_email(generation, Secret::new("a@b.test"), Secret::new("password"))
        .unwrap();
    assert_eq!(backend.snapshot().login, LoginState::LoggedIn);
    assert!(backend.snapshot().persistence_error);
    assert!(backend.user_token().is_some());
}

#[test]
fn profiles_preserve_baseline_selection_pin_and_primary_addon_rules() {
    let directory = Directory::new();
    seed(&directory, "member", None);
    let backend = backend(
        service(|request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(200, json!("shared-owner")),
            "/rest/v1/rpc/sync_pull_profiles" => response(
                200,
                json!([
                {"profile_index":7,"name":"Primary","is_primary":true,"profile_background_url":"https://image.invalid/bg"},
                {"profile_index":2,"name":"Second","uses_primary_addons":true},
                {"profile_index":3,"name":"Kids"}]),
            ),
            "/rest/v1/rpc/sync_pull_profile_locks" => response(
                200,
                json!([
                {"profile_id":7,"pin_enabled":false},{"profile_id":2,"pin_enabled":false},{"profile_id":3,"pin_enabled":true}]),
            ),
            "/rest/v1/rpc/verify_profile_pin" => {
                let body: serde_json::Value = serde_json::from_slice(&request.body).unwrap();
                assert_eq!(body["p_profile_id"], 3);
                response(200, json!(body["p_pin"] == "4271"))
            }
            _ => panic!("unexpected profile request"),
        }),
        &directory,
    );
    assert_eq!(backend.load_profiles().unwrap(), 3);
    assert_eq!(backend.snapshot().owner.as_deref(), Some("shared-owner"));
    assert!(backend.snapshot().selection.needs_selection());
    assert!(!backend.snapshot().selection.can_dismiss());
    backend
        .select_profile(ProfileId::new(2).unwrap(), None)
        .unwrap();
    let snapshot = backend.snapshot();
    assert_eq!(snapshot.selection.suggested_slot(), 1);
    assert_eq!(snapshot.selection.addon_profile(false).unwrap().get(), 7);
    assert!(!snapshot.selection.needs_selection());
    assert_eq!(
        backend.select_profile(ProfileId::new(3).unwrap(), Some(Secret::new("0000"))),
        Err(Error::WrongPin)
    );
    assert_eq!(backend.snapshot().selection.selected.unwrap().get(), 2);
    backend
        .select_profile(ProfileId::new(3).unwrap(), Some(Secret::new("4271")))
        .unwrap();
    assert_eq!(
        backend
            .snapshot()
            .selection
            .addon_profile(false)
            .unwrap()
            .get(),
        3
    );
    assert_eq!(
        backend
            .snapshot()
            .selection
            .addon_profile(true)
            .unwrap()
            .get(),
        7
    );
    assert!(backend.snapshot().selection.can_dismiss());
    let restored = directory.store().load().unwrap();
    assert!(restored.selection.needs_selection());
    assert!(!restored.selection.can_dismiss());
    assert!(!fs::read_to_string(directory.account_file())
        .unwrap()
        .contains("4271"));
    backend.logout();
    assert!(backend.snapshot().selection.profiles.is_empty());
    assert!(backend.snapshot().owner.is_none());
}

#[test]
fn unverified_profiles_are_blocked_and_owner_failure_does_not_use_member_id() {
    let directory = Directory::new();
    seed(&directory, "member", None);
    let backend = backend(
        service(|request| match request.url.path() {
            "/rest/v1/rpc/sync_pull_profiles" => {
                response(200, json!([{"profile_index":1,"name":"Primary"}]))
            }
            _ => Err(Error::Network),
        }),
        &directory,
    );
    backend.load_profiles().unwrap();
    assert_eq!(backend.snapshot().owner, None);
    assert_eq!(
        backend.snapshot().selection.profiles[0].lock,
        ProfileLock::Unknown
    );
    assert!(backend.snapshot().selection.needs_selection());
    assert_eq!(
        backend.select_profile(ProfileId::new(1).unwrap(), None),
        Err(Error::ProfileUnverified)
    );
}

#[test]
fn cached_verified_locks_owner_and_selection_survive_service_outages() {
    let directory = Directory::new();
    seed(&directory, "member", None);
    let failures = Arc::new(AtomicUsize::new(0));
    let mode = failures.clone();
    let backend = backend(
        service(move |request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" if mode.load(Ordering::SeqCst) == 0 => {
                response(200, json!("owner"))
            }
            "/rest/v1/rpc/sync_pull_profiles" => {
                response(200, json!([{"profile_index":1},{"profile_index":2}]))
            }
            "/rest/v1/rpc/sync_pull_profile_locks" if mode.load(Ordering::SeqCst) == 0 => response(
                200,
                json!([
            {"profile_id":1,"pin_enabled":false},{"profile_id":2,"pin_enabled":true}]),
            ),
            _ => Err(Error::Network),
        }),
        &directory,
    );
    backend.load_profiles().unwrap();
    backend
        .select_profile(ProfileId::new(1).unwrap(), None)
        .unwrap();
    failures.store(1, Ordering::SeqCst);
    backend.load_profiles().unwrap();
    assert_eq!(backend.snapshot().owner.as_deref(), Some("owner"));
    assert_eq!(
        backend.snapshot().selection.profiles[1].lock,
        ProfileLock::Locked
    );
    assert_eq!(
        backend.select_profile(ProfileId::new(2).unwrap(), None),
        Err(Error::WrongPin)
    );
    assert_eq!(
        backend.select_profile(ProfileId::new(2).unwrap(), Some(Secret::new("4271"))),
        Err(Error::Network)
    );
    assert_eq!(backend.snapshot().selection.selected.unwrap().get(), 1);
}

#[test]
fn tv_polling_uses_server_interval_timeout_and_retries_exchange_outages() {
    let directory = Directory::new();
    let clock = Arc::new(TestClock(AtomicU64::new(1000)));
    let exchanges = Arc::new(AtomicUsize::new(0));
    let count = exchanges.clone();
    let service = service(move |request| match request.url.path() {
        "/auth/v1/signup" => response(200, auth("anon", None, "anon")),
        "/rest/v1/rpc/start_tv_login_session" => response(
            200,
            json!({"code":"code","web_url":"https://login.invalid/tv","poll_interval_seconds":5}),
        ),
        "/rest/v1/rpc/poll_tv_login_session" => response(200, json!({"status":"authorized"})),
        "/functions/v1/tv-logins-exchange" => {
            if count.fetch_add(1, Ordering::SeqCst) == 0 {
                response(503, json!({}))
            } else {
                response(200, auth("user", None, "user"))
            }
        }
        _ => panic!("unexpected request"),
    });
    let backend = AccountBackend::new(service, Some(directory.store()), clock.clone());
    let generation = backend.begin_tv();
    backend.finish_tv_start(generation).unwrap();
    assert!(backend.claim_poll().is_none());
    clock.0.store(1005, Ordering::SeqCst);
    assert_eq!(backend.claim_poll(), Some(generation));
    assert!(backend.claim_poll().is_none());
    assert_eq!(backend.finish_poll(generation), Err(Error::Http(503)));
    assert_eq!(backend.snapshot().login, LoginState::Waiting);
    clock.0.store(1010, Ordering::SeqCst);
    assert_eq!(backend.claim_poll(), Some(generation));
    backend.finish_poll(generation).unwrap();
    assert_eq!(backend.snapshot().login, LoginState::LoggedIn);
    backend.logout();
    let generation = backend.begin_tv();
    backend.finish_tv_start(generation).unwrap();
    clock.0.store(1610, Ordering::SeqCst);
    assert!(backend.claim_poll().is_none());
    assert_eq!(backend.snapshot().error, Some(Error::CodeExpired));
}

#[test]
fn tv_polling_and_code_expiry_ignore_wall_clock_changes() {
    struct IndependentClock {
        wall: AtomicU64,
        elapsed: AtomicU64,
    }
    impl nuvio_backend::account::Clock for IndependentClock {
        fn seconds(&self) -> u64 {
            self.wall.load(Ordering::SeqCst)
        }
        fn monotonic_millis(&self) -> u64 {
            self.elapsed.load(Ordering::SeqCst)
        }
    }
    let directory = Directory::new();
    let clock = Arc::new(IndependentClock {
        wall: AtomicU64::new(1000),
        elapsed: AtomicU64::new(0),
    });
    let service = service(|request| match request.url.path() {
        "/auth/v1/signup" => response(200, auth("anon", None, "anon")),
        "/rest/v1/rpc/start_tv_login_session" => response(
            200,
            json!({"code":"code","web_url":"https://login.invalid/tv","poll_interval_seconds":5}),
        ),
        "/rest/v1/rpc/poll_tv_login_session" => response(200, json!({"status":"pending"})),
        _ => panic!("unexpected request"),
    });
    let backend = AccountBackend::new(service, Some(directory.store()), clock.clone());
    let generation = backend.begin_tv();
    backend.finish_tv_start(generation).unwrap();
    clock.wall.store(1_000_000, Ordering::SeqCst);
    clock.elapsed.store(4999, Ordering::SeqCst);
    assert!(backend.claim_poll().is_none());
    assert_eq!(backend.snapshot().login, LoginState::Waiting);
    clock.wall.store(1, Ordering::SeqCst);
    clock.elapsed.store(5000, Ordering::SeqCst);
    assert_eq!(backend.claim_poll(), Some(generation));
    backend.finish_poll(generation).unwrap();
    clock.elapsed.store(600_000, Ordering::SeqCst);
    assert!(backend.claim_poll().is_none());
    assert_eq!(backend.snapshot().error, Some(Error::CodeExpired));
}

#[test]
fn anonymous_rust_session_restores_without_a_signed_in_user() {
    let directory = Directory::new();
    let mut anonymous = session("anonymous", None, "anon");
    anonymous.anonymous = true;
    directory
        .store()
        .save(&AccountData {
            session: Some(anonymous),
            ..AccountData::default()
        })
        .unwrap();
    let backend = backend(
        service(|_| panic!("startup must not make an HTTP request")),
        &directory,
    );
    assert_eq!(backend.snapshot().login, LoginState::LoggedOut);
    assert!(backend.user_token().is_none());
    assert!(backend.user_id().is_empty());
    assert!(!backend.needs_selection());
}

#[test]
fn concurrent_expired_requests_refresh_before_sending_the_operation() {
    let directory = Directory::new();
    seed(&directory, "user", Some(999));
    let refreshes = Arc::new(AtomicUsize::new(0));
    let count = refreshes.clone();
    let backend = backend(
        service(move |request| {
            if request.url.query() == Some("grant_type=refresh_token") {
                count.fetch_add(1, Ordering::SeqCst);
                response(200, auth("user", Some(5000), "new"))
            } else {
                assert_eq!(
                    request.authorization.expose(),
                    token("user", Some(5000), "new")
                );
                response(200, json!({}))
            }
        }),
        &directory,
    );
    let start = Barrier::new(8);
    thread::scope(|scope| {
        let start = &start;
        let mut handles = Vec::new();
        for _ in 0..8 {
            let backend = &backend;
            handles.push(scope.spawn(move || {
                start.wait();
                backend.rpc("test_rpc", b"{}")
            }));
        }
        for handle in handles {
            assert_eq!(handle.join().unwrap().unwrap().status, 200);
        }
    });
    assert_eq!(refreshes.load(Ordering::SeqCst), 1);
}

#[test]
fn malformed_successful_refresh_response_does_not_delete_a_valid_session() {
    let directory = Directory::new();
    seed(&directory, "user", Some(999));
    let backend = backend(
        service(|_| response(200, auth("different-user", None, "wrong"))),
        &directory,
    );
    assert_eq!(
        backend.rpc("test_rpc", b"{}").err(),
        Some(Error::InvalidData)
    );
    assert_eq!(backend.user_id(), "user");
    assert_eq!(
        directory
            .store()
            .load()
            .unwrap()
            .session
            .unwrap()
            .refresh
            .expose(),
        "refresh-old"
    );
}

#[test]
fn cancelled_correct_pin_never_commits_profile_selection() {
    let directory = Directory::new();
    seed(&directory, "user", None);
    let started = Arc::new(Barrier::new(2));
    let release = Arc::new(Barrier::new(2));
    let ready = started.clone();
    let finish = release.clone();
    let backend = backend(
        service(move |request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(200, json!("owner")),
            "/rest/v1/rpc/sync_pull_profiles" => {
                response(200, json!([{"profile_index":1},{"profile_index":2}]))
            }
            "/rest/v1/rpc/sync_pull_profile_locks" => response(
                200,
                json!([{"profile_id":1,"pin_enabled":false},{"profile_id":2,"pin_enabled":true}]),
            ),
            "/rest/v1/rpc/verify_profile_pin" => {
                ready.wait();
                finish.wait();
                response(200, json!(true))
            }
            _ => panic!("unexpected request"),
        }),
        &directory,
    );
    backend.load_profiles().unwrap();
    backend
        .select_profile(ProfileId::new(1).unwrap(), None)
        .unwrap();
    let action = backend.begin_selection();
    thread::scope(|scope| {
        let handle = scope.spawn(|| {
            backend.finish_selection(
                action,
                ProfileId::new(2).unwrap(),
                Some(Secret::new("4271")),
            )
        });
        started.wait();
        backend.cancel_selection();
        release.wait();
        assert_eq!(handle.join().unwrap(), Err(Error::Cancelled));
    });
    assert_eq!(backend.active_id(), 1);
    assert_eq!(
        directory
            .store()
            .load()
            .unwrap()
            .selection
            .selected
            .unwrap()
            .get(),
        1
    );
}

#[test]
fn changed_owner_and_removed_profiles_invalidate_the_previous_selection() {
    let directory = Directory::new();
    seed(&directory, "member", None);
    let mode = Arc::new(AtomicUsize::new(0));
    let state = mode.clone();
    let backend = backend(
        service(move |request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(
                200,
                if state.load(Ordering::SeqCst) < 2 {
                    json!("owner")
                } else {
                    json!("another-owner")
                },
            ),
            "/rest/v1/rpc/sync_pull_profiles" => response(
                200,
                if state.load(Ordering::SeqCst) == 0 {
                    json!([{"profile_index":1},{"profile_index":2}])
                } else {
                    json!([{"profile_index":1},{"profile_index":3}])
                },
            ),
            "/rest/v1/rpc/sync_pull_profile_locks" => response(
                200,
                json!([{"profile_id":1,"pin_enabled":false},{"profile_id":2,"pin_enabled":false},{"profile_id":3,"pin_enabled":false}]),
            ),
            _ => panic!("unexpected request"),
        }),
        &directory,
    );
    backend.load_profiles().unwrap();
    backend
        .select_profile(ProfileId::new(2).unwrap(), None)
        .unwrap();
    mode.store(1, Ordering::SeqCst);
    backend.load_profiles().unwrap();
    assert!(backend.snapshot().selection.selected.is_none());
    assert!(backend.needs_selection());
    backend
        .select_profile(ProfileId::new(3).unwrap(), None)
        .unwrap();
    mode.store(2, Ordering::SeqCst);
    backend.load_profiles().unwrap();
    assert!(backend.snapshot().selection.selected.is_none());
    assert_eq!(backend.snapshot().owner.as_deref(), Some("another-owner"));
    assert!(backend.needs_selection());
}
