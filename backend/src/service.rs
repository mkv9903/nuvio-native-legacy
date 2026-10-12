use crate::{
    config::AccountConfig,
    error::{Error, Result},
    http::{Method, Request, Response, Transport, ACCOUNT_RESPONSE_LIMIT, REQUEST_RESPONSE_LIMIT},
    model::{Profile, ProfileId, ProfileLock, RemoteProfile, RemoteProfileId, Secret, Session},
};
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use serde_json::{json, Value};
use std::{collections::HashMap, sync::Arc};
use url::Url;
use uuid::Uuid;
use zeroize::Zeroizing;

#[derive(Clone)]
pub struct AccountService {
    config: AccountConfig,
    transport: Arc<dyn Transport>,
}

#[derive(Clone, Debug)]
pub struct LoginCode {
    pub code: String,
    pub web_url: String,
    pub interval_seconds: u64,
    pub nonce: Secret,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PollResult {
    Pending,
    Authorized,
    Expired,
}

impl AccountService {
    pub fn new(config: AccountConfig, transport: Arc<dyn Transport>) -> Self {
        Self { config, transport }
    }

    pub fn password_login(&self, email: &Secret, password: &Secret) -> Result<Session> {
        #[derive(Serialize)]
        struct Credentials<'a> {
            email: &'a str,
            password: &'a str,
        }
        let response = self.post(
            "auth/v1/token?grant_type=password",
            None,
            &Credentials {
                email: email.expose(),
                password: password.expose(),
            },
        )?;
        if response.success().is_err() {
            return Err(email_error(&response));
        }
        decode_session(&response, false)
    }

    pub fn refresh(&self, session: &Session) -> Result<Session> {
        if session.refresh.is_empty() {
            return Err(Error::InvalidSession);
        }
        #[derive(Serialize)]
        struct Refresh<'a> {
            refresh_token: &'a str,
        }
        let response = self.post(
            "auth/v1/token?grant_type=refresh_token",
            None,
            &Refresh {
                refresh_token: session.refresh.expose(),
            },
        )?;
        response.success()?;
        let fresh = decode_session(&response, session.anonymous).map_err(|_| Error::InvalidData)?;
        if fresh.subject != session.subject {
            return Err(Error::InvalidData);
        }
        Ok(fresh)
    }

    pub fn anonymous(&self) -> Result<Session> {
        let response = self.post(
            "auth/v1/signup",
            None,
            &json!({"data":{"tv_client":"webos"}}),
        )?;
        if response.success().is_ok() {
            if let Ok(session) = decode_session(&response, true) {
                return Ok(session);
            }
        }
        // Preserve outages instead of doubling the load with a grant fallback.
        if response.status == 429 || response.status >= 500 {
            return Err(Error::Http(response.status));
        }
        let response = self.post("auth/v1/token?grant_type=anonymous", None, &json!({}))?;
        response.success()?;
        decode_session(&response, true)
    }

    pub fn request_code(&self, anonymous: &Session) -> Result<LoginCode> {
        let nonce = Secret::new(Uuid::new_v4().to_string());
        #[derive(Serialize)]
        struct Start<'a> {
            p_device_nonce: &'a str,
            p_redirect_base_url: &'a str,
            #[serde(skip_serializing_if = "Option::is_none")]
            p_device_name: Option<&'a str>,
        }
        let mut request = Start {
            p_device_nonce: nonce.expose(),
            p_redirect_base_url: self.config.login_base.as_str(),
            p_device_name: Some("LG webOS (Nuvio Rust Test)"),
        };
        let mut response = self.rpc("start_tv_login_session", &request, &anonymous.access)?;
        #[derive(Deserialize)]
        struct ServiceError {
            code: Option<String>,
        }
        if serde_json::from_slice::<ServiceError>(&response.body)
            .ok()
            .is_some_and(|body| body.code.as_deref() == Some("PGRST202"))
        {
            request.p_device_name = None;
            response = self.rpc("start_tv_login_session", &request, &anonymous.access)?;
        }
        response.success()?;
        #[derive(Deserialize)]
        struct Code {
            code: String,
            web_url: Option<String>,
            poll_interval_seconds: Option<u64>,
        }
        let code: Code = decode(&response)?;
        if code.code.is_empty() {
            return Err(Error::InvalidData);
        }
        let web_url = code
            .web_url
            .unwrap_or_else(|| self.config.login_base.to_string());
        let url = Url::parse(&web_url).map_err(|_| Error::InvalidData)?;
        if !matches!(url.scheme(), "https" | "http") {
            return Err(Error::InvalidData);
        }
        Ok(LoginCode {
            code: code.code,
            web_url,
            interval_seconds: code
                .poll_interval_seconds
                .filter(|value| (1..=60).contains(value))
                .unwrap_or(3),
            nonce,
        })
    }

    pub fn poll_code(&self, code: &LoginCode, session: &Session) -> Result<PollResult> {
        #[derive(Serialize)]
        struct Poll<'a> {
            p_code: &'a str,
            p_device_nonce: &'a str,
        }
        let response = self.rpc(
            "poll_tv_login_session",
            &Poll {
                p_code: &code.code,
                p_device_nonce: code.nonce.expose(),
            },
            &session.access,
        )?;
        response.success()?;
        #[derive(Deserialize)]
        struct Status {
            status: String,
        }
        match decode::<Status>(&response)?.status.as_str() {
            "pending" => Ok(PollResult::Pending),
            "expired" | "cancelled" => Ok(PollResult::Expired),
            "" => Err(Error::InvalidData),
            _ => Ok(PollResult::Authorized),
        }
    }

    pub fn exchange_code(&self, code: &LoginCode, session: &Session) -> Result<Session> {
        #[derive(Serialize)]
        struct Exchange<'a> {
            code: &'a str,
            device_nonce: &'a str,
        }
        let response = self.post(
            "functions/v1/tv-logins-exchange",
            Some(&session.access),
            &Exchange {
                code: &code.code,
                device_nonce: code.nonce.expose(),
            },
        )?;
        response.success()?;
        decode_session(&response, false)
    }

    pub fn rpc<T: Serialize>(&self, name: &str, body: &T, token: &Secret) -> Result<Response> {
        validate_identifier(name)?;
        self.post(&format!("rest/v1/rpc/{name}"), Some(token), body)
    }

    pub fn raw_rpc(&self, name: &str, body: &[u8], token: &Secret) -> Result<Response> {
        validate_identifier(name)?;
        let _: Value = serde_json::from_slice(body).map_err(|_| Error::InvalidData)?;
        self.send(
            Method::Post,
            &format!("rest/v1/rpc/{name}"),
            Some(token),
            Zeroizing::new(body.to_vec()),
            REQUEST_RESPONSE_LIMIT,
        )
    }

    pub fn table(&self, name: &str, query: &str, token: &Secret) -> Result<Response> {
        validate_identifier(name)?;
        self.send(
            Method::Get,
            &format!("rest/v1/{name}?{query}"),
            Some(token),
            Zeroizing::new(Vec::new()),
            REQUEST_RESPONSE_LIMIT,
        )
    }

    pub fn owner(&self, token: &Secret) -> Result<String> {
        let response = self.rpc("get_sync_owner", &json!({}), token)?;
        response.success()?;
        let owner: String = decode(&response)?;
        if owner.is_empty() {
            return Err(Error::OwnerUnverified);
        }
        Ok(owner)
    }

    pub fn profiles(&self, token: &Secret) -> Result<Vec<Profile>> {
        let response = self.rpc("sync_pull_profiles", &json!({}), token)?;
        response.success()?;
        let values: Vec<Value> = decode(&response)?;
        let mut profiles = Vec::new();
        for value in values {
            if let Ok(profile) = serde_json::from_value::<RemoteProfile>(value)
                .map_err(|_| Error::InvalidData)
                .and_then(Profile::try_from)
            {
                if !profiles
                    .iter()
                    .any(|existing: &Profile| existing.id == profile.id)
                {
                    profiles.push(profile);
                }
            }
        }
        if profiles.is_empty() {
            return Err(Error::InvalidData);
        }
        Ok(profiles)
    }

    pub fn locks(&self, token: &Secret) -> Result<HashMap<ProfileId, ProfileLock>> {
        let response = self.rpc("sync_pull_profile_locks", &json!({}), token)?;
        response.success()?;
        #[derive(Deserialize)]
        struct Lock {
            profile_id: Option<RemoteProfileId>,
            profile_index: Option<u32>,
            pin_enabled: bool,
        }
        let records: Vec<Lock> = decode(&response)?;
        Ok(records
            .into_iter()
            .filter_map(|record| {
                record
                    .profile_id
                    .and_then(RemoteProfileId::numeric)
                    .or_else(|| record.profile_index.and_then(ProfileId::new))
                    .map(|id| {
                        (
                            id,
                            if record.pin_enabled {
                                ProfileLock::Locked
                            } else {
                                ProfileLock::Unlocked
                            },
                        )
                    })
            })
            .collect())
    }

    pub fn verify_pin(&self, id: ProfileId, pin: &Secret, token: &Secret) -> Result<bool> {
        #[derive(Serialize)]
        struct Pin<'a> {
            p_profile_id: u32,
            p_pin: &'a str,
        }
        let response = self.rpc(
            "verify_profile_pin",
            &Pin {
                p_profile_id: id.get(),
                p_pin: pin.expose(),
            },
            token,
        )?;
        response.success()?;
        decode(&response)
    }

    pub fn avatars(&self) -> Result<HashMap<String, String>> {
        let response = self.rpc("get_avatar_catalog", &json!({}), &self.config.public_key)?;
        response.success()?;
        #[derive(Deserialize)]
        struct Avatar {
            id: String,
            storage_path: String,
        }
        let records: Vec<Avatar> = decode(&response)?;
        let mut avatars = HashMap::new();
        for record in records {
            if record.id.is_empty()
                || record.storage_path.is_empty()
                || record.storage_path.split('/').any(|part| part == "..")
            {
                continue;
            }
            let mut url = self
                .config
                .base
                .join("storage/v1/object/public/avatars/")
                .map_err(|_| Error::Configuration)?;
            {
                let mut segments = url.path_segments_mut().map_err(|_| Error::Configuration)?;
                segments.pop_if_empty();
                segments.extend(record.storage_path.split('/'));
            }
            avatars.insert(record.id, url.to_string());
        }
        Ok(avatars)
    }

    fn post<T: Serialize>(&self, path: &str, token: Option<&Secret>, body: &T) -> Result<Response> {
        self.send(
            Method::Post,
            path,
            token,
            Zeroizing::new(serde_json::to_vec(body).map_err(|_| Error::InvalidData)?),
            ACCOUNT_RESPONSE_LIMIT,
        )
    }
    fn send(
        &self,
        method: Method,
        path: &str,
        token: Option<&Secret>,
        body: Zeroizing<Vec<u8>>,
        response_limit: u64,
    ) -> Result<Response> {
        let url = self
            .config
            .base
            .join(path)
            .map_err(|_| Error::Configuration)?;
        if url.origin() != self.config.base.origin() {
            return Err(Error::Configuration);
        }
        self.transport.send(Request {
            method,
            url,
            public_key: self.config.public_key.clone(),
            authorization: token.unwrap_or(&self.config.public_key).clone(),
            body,
            response_limit,
        })
    }
}

pub fn decode<T: DeserializeOwned>(response: &Response) -> Result<T> {
    serde_json::from_slice(&response.body).map_err(|_| Error::InvalidData)
}

fn decode_session(response: &Response, anonymous: bool) -> Result<Session> {
    #[derive(Deserialize)]
    struct Tokens {
        access_token: Secret,
        refresh_token: Secret,
    }
    #[derive(Deserialize)]
    #[serde(untagged)]
    enum Envelope {
        Root(Tokens),
        Nested { session: Tokens },
    }
    let tokens = match decode(response)? {
        Envelope::Root(tokens) | Envelope::Nested { session: tokens } => tokens,
    };
    if tokens.refresh_token.is_empty() {
        return Err(Error::InvalidSession);
    }
    Session::from_tokens(tokens.access_token, tokens.refresh_token, anonymous)
}

pub fn email_error(response: &Response) -> Error {
    #[derive(Deserialize, Default)]
    struct Failure {
        #[serde(default)]
        error_code: String,
        #[serde(default)]
        error: String,
        #[serde(default)]
        error_description: String,
        #[serde(default)]
        msg: String,
        #[serde(default)]
        message: String,
    }
    if response.status >= 500 {
        return Error::Http(response.status);
    }
    let body: Failure = decode(response).unwrap_or_default();
    let code = if body.error_code.is_empty() {
        body.error
    } else {
        body.error_code
    };
    let description = if !body.error_description.is_empty() {
        body.error_description
    } else if !body.msg.is_empty() {
        body.msg
    } else {
        body.message
    };
    if response.status == 429
        || matches!(
            code.as_str(),
            "over_request_rate_limit" | "over_email_send_rate_limit"
        )
    {
        Error::Http(429)
    } else if code == "email_not_confirmed" || description.contains("not confirmed") {
        Error::EmailUnconfirmed
    } else if response.status == 400
        || matches!(code.as_str(), "invalid_credentials" | "invalid_grant")
        || description.contains("Invalid login credentials")
    {
        Error::InvalidCredentials
    } else {
        Error::Http(response.status)
    }
}

fn validate_identifier(value: &str) -> Result<()> {
    if value.is_empty()
        || !value
            .bytes()
            .all(|byte| byte.is_ascii_alphanumeric() || byte == b'_')
    {
        return Err(Error::InvalidData);
    }
    Ok(())
}
