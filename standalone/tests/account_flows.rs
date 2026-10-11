mod support;
use nuvio_rust_app::{
    account::{BackendConfig, CloudClient, Session, SessionVault},
    error::AppError,
    secret::Secret,
};
use serde_json::{json, Value};
use std::sync::{
    atomic::{AtomicUsize, Ordering},
    mpsc, Arc, Barrier, Mutex,
};
use support::{fake, response, tokens, FixtureDirectory};

fn config() -> Arc<BackendConfig> {
    Arc::new(
        BackendConfig::new(
            "https://backend.example.invalid",
            "fixture-anon",
            "https://login.example.invalid/tv",
        )
        .unwrap(),
    )
}
fn session(expires_at: u64) -> Session {
    Session {
        access_token: Secret::new("fixture-old-access"),
        refresh_token: Secret::new("fixture-old-refresh"),
        expires_at,
        user_id: "fixture-account".to_owned(),
    }
}

#[test]
fn password_login_serializes_special_characters_and_does_not_install_itself() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let transport = fake(|request| {
        assert_eq!(request.url.path(), "/auth/v1/token");
        assert_eq!(request.url.query(), Some("grant_type=password"));
        assert_eq!(request.header_value("apikey"), Some("fixture-anon"));
        let body: Value = serde_json::from_slice(request.body_bytes()).unwrap();
        assert_eq!(body["password"], "pass\"word\\with=equals");
        Ok(response(
            200,
            json!({"session":tokens("new-access", "new-refresh")}),
        ))
    });
    let cloud = CloudClient::new(config(), transport, vault.clone());
    let result = cloud
        .password_login(
            "test@example.invalid",
            &Secret::new("pass\"word\\with=equals"),
        )
        .unwrap();
    assert_eq!(result.access_token.expose(), "new-access");
    assert!(vault.snapshot().is_none());
    assert_eq!(format!("{:?}", result.access_token), "Secret([redacted])");
}

#[test]
fn device_login_uses_uuid_nonce_and_exchanges_only_after_approval() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let stage = AtomicUsize::new(0);
    let nonce = Mutex::new(String::new());
    let transport = fake(move |request| {
        let body: Value = serde_json::from_slice(request.body_bytes()).unwrap();
        match stage.fetch_add(1, Ordering::SeqCst) {
            0 => {
                assert_eq!(request.url.path(), "/auth/v1/signup");
                Ok(response(
                    200,
                    tokens("anonymous-access", "anonymous-refresh"),
                ))
            }
            1 => {
                assert_eq!(request.url.path(), "/rest/v1/rpc/start_tv_login_session");
                let value = body["p_device_nonce"].as_str().unwrap();
                assert_eq!(uuid::Uuid::parse_str(value).unwrap().get_version_num(), 4);
                *nonce.lock().unwrap() = value.to_owned();
                assert_eq!(body["p_device_name"], "LG webOS (Nuvio Rust Test)");
                Ok(response(
                    200,
                    json!({"code":"ABC123","poll_interval_seconds":2}),
                ))
            }
            2 | 3 => {
                assert_eq!(request.url.path(), "/rest/v1/rpc/poll_tv_login_session");
                assert_eq!(body["p_code"], "ABC123");
                assert_eq!(body["p_device_nonce"], *nonce.lock().unwrap());
                Ok(response(
                    200,
                    json!({"status":if stage.load(Ordering::SeqCst) == 3 {"pending"} else {"approved"}}),
                ))
            }
            4 => {
                assert_eq!(request.url.path(), "/functions/v1/tv-logins-exchange");
                assert_eq!(body["device_nonce"], *nonce.lock().unwrap());
                Ok(response(200, tokens("account-access", "account-refresh")))
            }
            _ => panic!("unexpected request"),
        }
    });
    let cloud = CloudClient::new(config(), transport, vault);
    let login = cloud.start_device_login().unwrap();
    assert_eq!(
        login.web_url,
        "https://login.example.invalid/tv?code=ABC123"
    );
    assert_eq!(login.poll_interval_seconds, 2);
    assert!(cloud.poll_device_login(&login).unwrap().is_none());
    assert_eq!(
        cloud
            .poll_device_login(&login)
            .unwrap()
            .unwrap()
            .access_token
            .expose(),
        "account-access"
    );
}

#[test]
fn concurrent_expired_requests_share_one_refresh() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let generation = vault.install(vault.generation(), session(1)).unwrap();
    let refreshes = Arc::new(AtomicUsize::new(0));
    let counter = refreshes.clone();
    let transport = fake(move |request| {
        if request.url.path() == "/auth/v1/token" {
            counter.fetch_add(1, Ordering::SeqCst);
            return Ok(response(
                200,
                tokens("fixture-new-access", "fixture-new-refresh"),
            ));
        }
        assert_eq!(
            request.header_value("authorization"),
            Some("Bearer fixture-new-access")
        );
        Ok(response(200, json!({"ready":true})))
    });
    let cloud = Arc::new(CloudClient::new(config(), transport, vault));
    let gate = Arc::new(Barrier::new(4));
    let mut threads = Vec::new();
    for _ in 0..4 {
        let cloud = cloud.clone();
        let gate = gate.clone();
        threads.push(std::thread::spawn(move || {
            gate.wait();
            cloud
                .rpc::<Value>(generation, "fixture", json!({}))
                .unwrap()
        }));
    }
    for thread in threads {
        assert_eq!(thread.join().unwrap()["ready"], true);
    }
    assert_eq!(refreshes.load(Ordering::SeqCst), 1);
}

#[test]
fn refresh_server_failure_preserves_login_but_rejection_clears_it() {
    for (status, expected) in [
        (503, AppError::Http(503)),
        (429, AppError::Http(429)),
        (400, AppError::SessionExpired),
    ] {
        let directory = FixtureDirectory::new();
        let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
        let generation = vault.install(vault.generation(), session(1)).unwrap();
        let cloud = CloudClient::new(
            config(),
            fake(move |_| Ok(response(status, json!({})))),
            vault.clone(),
        );
        assert_eq!(
            cloud
                .rpc::<Value>(generation, "fixture", json!({}))
                .unwrap_err(),
            expected
        );
        assert_eq!(vault.snapshot().is_some(), status != 400);
        assert_eq!(
            directory
                .store()
                .read::<Session>("session.json")
                .unwrap()
                .is_some(),
            status != 400
        );
    }
}

#[test]
fn refresh_finishing_after_logout_cannot_restore_the_session() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let generation = vault.install(vault.generation(), session(1)).unwrap();
    let (entered_send, entered) = mpsc::channel();
    let (release, released) = mpsc::channel();
    let released = Mutex::new(released);
    let transport = fake(move |_| {
        entered_send.send(()).unwrap();
        released.lock().unwrap().recv().unwrap();
        Ok(response(200, tokens("late-access", "late-refresh")))
    });
    let cloud = Arc::new(CloudClient::new(config(), transport, vault.clone()));
    let thread = std::thread::spawn(move || cloud.rpc::<Value>(generation, "fixture", json!({})));
    entered
        .recv_timeout(std::time::Duration::from_secs(2))
        .unwrap();
    vault.logout().unwrap();
    release.send(()).unwrap();
    assert_eq!(
        thread.join().unwrap().unwrap_err(),
        AppError::SessionExpired
    );
    assert!(vault.snapshot().is_none());
    assert!(!directory.path("session.json").exists());
}

#[test]
fn late_login_and_refresh_from_a_different_account_are_rejected() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let old = vault.generation();
    vault.logout().unwrap();
    assert_eq!(
        vault.install(old, session(1)).unwrap_err(),
        AppError::SessionExpired
    );
    let generation = vault.install(vault.generation(), session(1)).unwrap();
    let cloud = CloudClient::new(
        config(),
        fake(|_| {
            let mut body = tokens("wrong-account-access", "wrong-account-refresh");
            body["user"]["id"] = json!("another-account");
            Ok(response(200, body))
        }),
        vault.clone(),
    );
    assert_eq!(
        cloud
            .rpc::<Value>(generation, "fixture", json!({}))
            .unwrap_err(),
        AppError::InvalidResponse
    );
    assert_eq!(vault.snapshot().unwrap().1.user_id, "fixture-account");
}

#[test]
fn fresh_storage_ignores_legacy_files_and_uses_private_permissions() {
    let directory = FixtureDirectory::new();
    std::fs::write(
        directory.path("legacy-session.txt"),
        "unrelated legacy fixture",
    )
    .unwrap();
    let store = directory.store();
    let vault = SessionVault::new(store.clone()).unwrap();
    assert!(vault.snapshot().is_none());
    vault.install(vault.generation(), session(1)).unwrap();
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            std::fs::metadata(directory.path("session.json"))
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o600
        );
    }
    assert!(store.read::<Value>("../session.json").is_err());
    vault.logout().unwrap();
    assert!(SessionVault::new(store).unwrap().snapshot().is_none());
}
