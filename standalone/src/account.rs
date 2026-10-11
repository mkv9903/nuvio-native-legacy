//! Nuvio's existing service contracts, implemented directly with Serde and HTTP.
use crate::{
    error::{AppError, Result},
    http::{parse_service_url, HttpRequest, HttpResponse, Transport},
    secret::Secret,
    service_config,
    storage::DataStore,
};
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::{
    sync::{Arc, Mutex},
    time::{SystemTime, UNIX_EPOCH},
};
use url::Url;

const SESSION_FILE: &str = "session.json";

fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

pub struct BackendConfig {
    base: Url,
    key: Secret,
    login_base: Url,
}

impl BackendConfig {
    pub fn embedded() -> Result<Self> {
        Self::new(
            service_config::SUPABASE_URL,
            service_config::SUPABASE_ANON_KEY,
            service_config::TV_LOGIN_BASE,
        )
    }

    pub fn new(base: &str, key: &str, login_base: &str) -> Result<Self> {
        let mut base = parse_service_url(base).map_err(|_| AppError::Configuration)?;
        let login_base = parse_service_url(login_base).map_err(|_| AppError::Configuration)?;
        if key.is_empty()
            || base.query().is_some()
            || !secure_account_url(&base)
            || !secure_account_url(&login_base)
        {
            return Err(AppError::Configuration);
        }
        let path = format!("{}/", base.path().trim_end_matches('/'));
        base.set_path(&path);
        Ok(Self {
            base,
            key: Secret::new(key),
            login_base,
        })
    }

    fn endpoint(&self, path: &str) -> Result<Url> {
        let url = self
            .base
            .join(path.trim_start_matches('/'))
            .map_err(|_| AppError::InvalidUrl)?;
        if self.base.origin() != url.origin() {
            return Err(AppError::InvalidUrl);
        }
        Ok(url)
    }

    fn request(&self, path: &str, body: Option<&Value>, token: &Secret) -> Result<HttpRequest> {
        let url = self.endpoint(path)?;
        let mut request = match body {
            Some(value) => HttpRequest::json(url, value)?,
            None => HttpRequest::get(url),
        };
        request.header("apikey", self.key.expose())?;
        request.header("authorization", &format!("Bearer {}", token.expose()))?;
        Ok(request)
    }
}

fn secure_account_url(url: &Url) -> bool {
    url.scheme() == "https" || matches!(url.host_str(), Some("localhost" | "127.0.0.1" | "[::1]"))
}

#[derive(Clone, Serialize, Deserialize)]
pub struct Session {
    pub access_token: Secret,
    pub refresh_token: Secret,
    pub expires_at: u64,
    pub user_id: String,
}

#[derive(Deserialize)]
struct RawSession {
    access_token: Secret,
    refresh_token: Secret,
    expires_at: Option<u64>,
    expires_in: Option<u64>,
    user: Option<AuthUser>,
}

#[derive(Deserialize)]
struct AuthUser {
    id: String,
}

#[derive(Default, Deserialize)]
struct Claims {
    #[serde(default)]
    sub: String,
    #[serde(default)]
    exp: u64,
}

#[derive(Deserialize)]
#[serde(untagged)]
enum AuthResponse {
    Nested { session: RawSession },
    Root(RawSession),
}

impl Session {
    fn from_response(response: &HttpResponse) -> Result<Self> {
        let raw = match response.json::<AuthResponse>()? {
            AuthResponse::Nested { session } | AuthResponse::Root(session) => session,
        };
        if raw.access_token.is_empty() || raw.refresh_token.is_empty() {
            return Err(AppError::InvalidResponse);
        }
        let claims = raw
            .access_token
            .expose()
            .split('.')
            .nth(1)
            .and_then(|encoded| URL_SAFE_NO_PAD.decode(encoded).ok())
            .and_then(|bytes| serde_json::from_slice::<Claims>(&bytes).ok())
            .unwrap_or_default();
        let user_id = raw
            .user
            .map(|user| user.id)
            .filter(|id| !id.is_empty())
            .unwrap_or(claims.sub);
        if user_id.is_empty() || user_id.len() > 128 {
            return Err(AppError::InvalidResponse);
        }
        let expires_at = raw
            .expires_at
            .filter(|v| *v > 0)
            .or_else(|| raw.expires_in.map(|seconds| now().saturating_add(seconds)))
            .unwrap_or(claims.exp);
        if expires_at == 0 {
            return Err(AppError::InvalidResponse);
        }
        Ok(Self {
            access_token: raw.access_token,
            refresh_token: raw.refresh_token,
            expires_at,
            user_id,
        })
    }

    fn valid(&self) -> bool {
        !self.access_token.is_empty()
            && !self.refresh_token.is_empty()
            && !self.user_id.is_empty()
            && self.expires_at > 0
    }
}

struct AuthState {
    generation: u64,
    session: Option<Session>,
}

/// Persistence and generations share one lock. A late refresh cannot restore a logged-out session.
pub struct SessionVault {
    store: DataStore,
    state: Mutex<AuthState>,
}

impl SessionVault {
    pub fn new(store: DataStore) -> Result<Self> {
        let session = store.read::<Session>(SESSION_FILE)?.filter(Session::valid);
        Ok(Self {
            store,
            state: Mutex::new(AuthState {
                generation: 1,
                session,
            }),
        })
    }

    pub fn generation(&self) -> u64 {
        self.state.lock().expect("session state").generation
    }

    pub fn snapshot(&self) -> Option<(u64, Session)> {
        let state = self.state.lock().expect("session state");
        state
            .session
            .clone()
            .map(|session| (state.generation, session))
    }

    pub fn install(&self, expected_generation: u64, session: Session) -> Result<u64> {
        if !session.valid() {
            return Err(AppError::InvalidResponse);
        }
        let mut state = self.state.lock().expect("session state");
        if state.generation != expected_generation {
            return Err(AppError::SessionExpired);
        }
        let generation = state.generation.checked_add(1).ok_or(AppError::Busy)?;
        self.store.write(SESSION_FILE, &session)?;
        state.generation = generation;
        state.session = Some(session);
        Ok(generation)
    }

    pub fn logout(&self) -> Result<()> {
        let mut state = self.state.lock().expect("session state");
        state.generation = state.generation.checked_add(1).ok_or(AppError::Busy)?;
        state.session = None;
        self.store.remove(SESSION_FILE)
    }

    fn current(&self, expected_generation: u64) -> Result<Session> {
        let state = self.state.lock().expect("session state");
        if state.generation != expected_generation {
            return Err(AppError::SessionExpired);
        }
        state.session.clone().ok_or(AppError::SessionExpired)
    }

    fn refresh_commit(&self, generation: u64, observed: &Secret, session: Session) -> Result<()> {
        let mut state = self.state.lock().expect("session state");
        let old = state.session.as_ref().ok_or(AppError::SessionExpired)?;
        if state.generation != generation {
            return Err(AppError::SessionExpired);
        }
        if old.access_token.expose() != observed.expose() {
            return Ok(());
        }
        if old.user_id != session.user_id {
            return Err(AppError::InvalidResponse);
        }
        self.store.write(SESSION_FILE, &session)?;
        state.session = Some(session);
        Ok(())
    }

    fn reject_refresh(&self, generation: u64, observed: &Secret) -> Result<()> {
        let mut state = self.state.lock().expect("session state");
        if state.generation == generation
            && state
                .session
                .as_ref()
                .is_some_and(|s| s.access_token.expose() == observed.expose())
        {
            state.generation = state.generation.checked_add(1).ok_or(AppError::Busy)?;
            state.session = None;
            self.store.remove(SESSION_FILE)?;
        }
        Ok(())
    }
}

pub struct CloudClient {
    config: Arc<BackendConfig>,
    transport: Arc<dyn Transport>,
    pub vault: Arc<SessionVault>,
    refresh_lock: Mutex<()>,
}

impl CloudClient {
    pub fn new(
        config: Arc<BackendConfig>,
        transport: Arc<dyn Transport>,
        vault: Arc<SessionVault>,
    ) -> Self {
        Self {
            config,
            transport,
            vault,
            refresh_lock: Mutex::new(()),
        }
    }

    pub fn password_login(&self, email: &str, password: &Secret) -> Result<Session> {
        let response = self.raw_post(
            "auth/v1/token?grant_type=password",
            &json!({"email": email, "password": password}),
            &self.config.key,
        )?;
        if !(200..300).contains(&response.status) {
            return Err(login_error(&response));
        }
        Session::from_response(&response)
    }

    fn raw_post(&self, path: &str, body: &Value, token: &Secret) -> Result<HttpResponse> {
        self.transport
            .send(self.config.request(path, Some(body), token)?)
    }

    fn refresh(&self, generation: u64, observed: &Secret) -> Result<()> {
        let _single_refresh = self.refresh_lock.lock().expect("refresh state");
        let session = self.vault.current(generation)?;
        if session.access_token.expose() != observed.expose() {
            return Ok(());
        }
        let response = self.raw_post(
            "auth/v1/token?grant_type=refresh_token",
            &json!({"refresh_token": session.refresh_token}),
            &self.config.key,
        )?;
        if matches!(response.status, 400 | 401 | 403) {
            self.vault.reject_refresh(generation, observed)?;
            return Err(AppError::SessionExpired);
        }
        let new_session = Session::from_response(&response)?;
        self.vault.refresh_commit(generation, observed, new_session)
    }

    fn authenticated(
        &self,
        generation: u64,
        path: &str,
        body: Option<&Value>,
    ) -> Result<HttpResponse> {
        let mut session = self.vault.current(generation)?;
        if session.expires_at <= now().saturating_add(60) {
            self.refresh(generation, &session.access_token)?;
            session = self.vault.current(generation)?;
        }
        let response =
            self.transport
                .send(self.config.request(path, body, &session.access_token)?)?;
        self.vault.current(generation)?;
        if response.status != 401 {
            return Ok(response);
        }
        self.refresh(generation, &session.access_token)?;
        let session = self.vault.current(generation)?;
        let response =
            self.transport
                .send(self.config.request(path, body, &session.access_token)?)?;
        self.vault.current(generation)?;
        Ok(response)
    }

    pub fn rpc<T: serde::de::DeserializeOwned>(
        &self,
        generation: u64,
        name: &str,
        body: Value,
    ) -> Result<T> {
        if !name.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_') {
            return Err(AppError::InvalidRequest);
        }
        self.authenticated(generation, &format!("rest/v1/rpc/{name}"), Some(&body))?
            .json()
    }

    pub fn table<T: serde::de::DeserializeOwned>(
        &self,
        generation: u64,
        name: &str,
        query: &[(&str, String)],
    ) -> Result<T> {
        if !name.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'_') {
            return Err(AppError::InvalidRequest);
        }
        let mut endpoint = self.config.endpoint(&format!("rest/v1/{name}"))?;
        endpoint
            .query_pairs_mut()
            .extend_pairs(query.iter().map(|(name, value)| (*name, value.as_str())));
        let relative = format!("rest/v1/{name}?{}", endpoint.query().unwrap_or_default());
        self.authenticated(generation, &relative, None)?.json()
    }

    pub fn start_device_login(&self) -> Result<DeviceLogin> {
        let response = self.raw_post(
            "auth/v1/signup",
            &json!({"data":{"tv_client":"webos"}}),
            &self.config.key,
        )?;
        let session = match Session::from_response(&response) {
            Ok(session) => session,
            Err(_) if matches!(response.status, 400 | 403 | 404 | 422) => {
                Session::from_response(&self.raw_post(
                    "auth/v1/token?grant_type=anonymous",
                    &json!({}),
                    &self.config.key,
                )?)?
            }
            Err(error) => return Err(error),
        };
        let nonce = uuid::Uuid::new_v4().to_string();
        let mut body = json!({"p_device_nonce":nonce,"p_redirect_base_url":self.config.login_base.as_str(),"p_device_name":"LG webOS (Nuvio Rust Test)"});
        let mut response = self.raw_post(
            "rest/v1/rpc/start_tv_login_session",
            &body,
            &session.access_token,
        )?;
        let error_code = serde_json::from_slice::<Value>(&response.body)
            .ok()
            .and_then(|value| value["code"].as_str().map(str::to_owned));
        if matches!(response.status, 400 | 404) && error_code.as_deref() == Some("PGRST202") {
            body.as_object_mut()
                .expect("login request")
                .remove("p_device_name");
            response = self.raw_post(
                "rest/v1/rpc/start_tv_login_session",
                &body,
                &session.access_token,
            )?;
        }
        let code: LoginCode = response.json()?;
        if code.code.is_empty() || code.code.len() > 64 {
            return Err(AppError::InvalidResponse);
        }
        let web_url = match code.web_url.filter(|v| !v.is_empty()) {
            Some(value) => parse_service_url(&value)?,
            None => {
                let mut url = self.config.login_base.clone();
                url.query_pairs_mut().append_pair("code", &code.code);
                url
            }
        };
        if !secure_account_url(&web_url) {
            return Err(AppError::InvalidResponse);
        }
        Ok(DeviceLogin {
            code: code.code,
            web_url: web_url.to_string(),
            poll_interval_seconds: code.poll_interval_seconds.unwrap_or(3).clamp(1, 60),
            nonce,
            anonymous_token: session.access_token,
        })
    }

    pub fn poll_device_login(&self, login: &DeviceLogin) -> Result<Option<Session>> {
        #[derive(Deserialize)]
        struct Poll {
            status: String,
        }
        let poll: Poll = self
            .raw_post(
                "rest/v1/rpc/poll_tv_login_session",
                &json!({"p_code":login.code,"p_device_nonce":login.nonce}),
                &login.anonymous_token,
            )?
            .json()?;
        match poll.status.as_str() {
            "pending" => Ok(None),
            "expired" | "cancelled" => Err(AppError::LoginExpired),
            "" => Err(AppError::InvalidResponse),
            // Server versions use different completion labels. The exchange
            // endpoint verifies the code and nonce before issuing account tokens.
            _ => {
                let response = self.raw_post(
                    "functions/v1/tv-logins-exchange",
                    &json!({"code":login.code,"device_nonce":login.nonce}),
                    &login.anonymous_token,
                )?;
                Session::from_response(&response).map(Some)
            }
        }
    }
}

#[derive(Deserialize)]
struct LoginCode {
    code: String,
    web_url: Option<String>,
    poll_interval_seconds: Option<u64>,
}

pub struct DeviceLogin {
    pub code: String,
    pub web_url: String,
    pub poll_interval_seconds: u64,
    nonce: String,
    anonymous_token: Secret,
}

fn login_error(response: &HttpResponse) -> AppError {
    let body: Value = serde_json::from_slice(&response.body).unwrap_or(Value::Null);
    let code = body["error_code"]
        .as_str()
        .or(body["error"].as_str())
        .unwrap_or_default();
    match code {
        "email_not_confirmed" => AppError::EmailUnconfirmed,
        "invalid_credentials" | "invalid_grant" => AppError::InvalidCredentials,
        _ if response.status == 400 => AppError::InvalidCredentials,
        _ => AppError::Http(response.status),
    }
}
