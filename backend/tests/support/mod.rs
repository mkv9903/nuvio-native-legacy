#![allow(dead_code)]
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use nuvio_backend::{
    account::{AccountBackend, Clock},
    config::AccountConfig,
    error::Result,
    http::{Request, Response, Transport},
    model::{Secret, Session},
    service::AccountService,
    storage::AccountStore,
    TEST_APP_ID,
};
use serde_json::{json, Value};
use std::{
    fs,
    path::PathBuf,
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc,
    },
};
use uuid::Uuid;
use zeroize::Zeroizing;

pub struct Directory {
    pub parent: PathBuf,
    pub path: PathBuf,
}
impl Directory {
    pub fn new() -> Self {
        let parent = std::env::temp_dir().join(format!("nuvio-backend-{}", Uuid::new_v4()));
        let path = parent.join(TEST_APP_ID);
        fs::create_dir_all(&path).unwrap();
        Self { parent, path }
    }
    pub fn store(&self) -> AccountStore {
        AccountStore::open(&self.path, TEST_APP_ID).unwrap()
    }
}
impl Drop for Directory {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.parent);
    }
}

pub fn token(subject: &str, expiration: Option<u64>, tag: &str) -> String {
    let mut claims = json!({"sub": subject});
    if let Some(expiration) = expiration {
        claims["exp"] = json!(expiration);
    }
    format!(
        "header.{}.{}",
        URL_SAFE_NO_PAD.encode(serde_json::to_vec(&claims).unwrap()),
        tag
    )
}
pub fn session(subject: &str, expiration: Option<u64>, tag: &str) -> Session {
    Session::from_tokens(
        Secret::new(token(subject, expiration, tag)),
        Secret::new(format!("refresh-{tag}")),
        false,
    )
    .unwrap()
}
pub fn auth(subject: &str, expiration: Option<u64>, tag: &str) -> Value {
    json!({"access_token":token(subject, expiration, tag),"refresh_token":format!("refresh-{tag}")})
}
pub fn response(status: u16, body: Value) -> Result<Response> {
    Ok(Response {
        status,
        body: Zeroizing::new(serde_json::to_vec(&body).unwrap()),
    })
}
pub struct FakeTransport(pub Box<dyn Fn(Request) -> Result<Response> + Send + Sync>);
impl Transport for FakeTransport {
    fn send(&self, request: Request) -> Result<Response> {
        (self.0)(request)
    }
}
pub fn service(
    callback: impl Fn(Request) -> Result<Response> + Send + Sync + 'static,
) -> AccountService {
    AccountService::new(
        AccountConfig::new(
            "https://account.invalid",
            "public-test-key",
            "https://login.invalid/tv",
        )
        .unwrap(),
        Arc::new(FakeTransport(Box::new(callback))),
    )
}
pub struct TestClock(pub AtomicU64);
impl Clock for TestClock {
    fn seconds(&self) -> u64 {
        self.0.load(Ordering::SeqCst)
    }
}
pub fn backend(service: AccountService, directory: &Directory) -> AccountBackend {
    AccountBackend::new(
        service,
        Some(directory.store()),
        Arc::new(TestClock(AtomicU64::new(1000))),
    )
}
