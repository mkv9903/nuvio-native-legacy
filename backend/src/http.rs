use crate::{
    error::{Error, Result},
    model::Secret,
};
use std::time::Duration;
use url::Url;
use zeroize::Zeroizing;

pub const ACCOUNT_RESPONSE_LIMIT: u64 = 1024 * 1024;
pub const REQUEST_RESPONSE_LIMIT: u64 = 4 * 1024 * 1024;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Method {
    Get,
    Post,
}
pub struct Request {
    pub method: Method,
    pub url: Url,
    pub public_key: Secret,
    pub authorization: Secret,
    pub body: Zeroizing<Vec<u8>>,
    pub response_limit: u64,
}
pub struct Response {
    pub status: u16,
    pub body: Zeroizing<Vec<u8>>,
}
impl Response {
    pub fn success(&self) -> Result<()> {
        if (200..300).contains(&self.status) {
            Ok(())
        } else {
            Err(Error::Http(self.status))
        }
    }
}

pub trait Transport: Send + Sync {
    fn send(&self, request: Request) -> Result<Response>;
}

pub struct HttpTransport {
    agent: ureq::Agent,
}
impl Default for HttpTransport {
    fn default() -> Self {
        let agent = ureq::Agent::config_builder()
            .timeout_global(Some(Duration::from_secs(25)))
            .timeout_connect(Some(Duration::from_secs(10)))
            .http_status_as_error(false)
            .max_redirects(0)
            .max_redirects_will_error(false)
            .max_idle_connections(4)
            .max_idle_connections_per_host(4)
            .user_agent("Nuvio Rust Test")
            .build()
            .new_agent();
        Self { agent }
    }
}
impl Transport for HttpTransport {
    fn send(&self, request: Request) -> Result<Response> {
        let authorization = Zeroizing::new(format!("Bearer {}", request.authorization.expose()));
        let mut response = match request.method {
            Method::Get => self
                .agent
                .get(request.url.as_str())
                .header("apikey", request.public_key.expose())
                .header("Authorization", authorization.as_str())
                .header("Accept", "application/json")
                .call(),
            Method::Post => self
                .agent
                .post(request.url.as_str())
                .header("apikey", request.public_key.expose())
                .header("Authorization", authorization.as_str())
                .header("Accept", "application/json")
                .header("Content-Type", "application/json")
                .send(request.body.as_slice()),
        }
        .map_err(map_transport_error)?;
        let status = response.status().as_u16();
        let body = response
            .body_mut()
            .with_config()
            .limit(request.response_limit)
            .read_to_vec()
            .map_err(map_transport_error)?;
        Ok(Response {
            status,
            body: Zeroizing::new(body),
        })
    }
}

fn map_transport_error(error: ureq::Error) -> Error {
    // Library diagnostics can contain URLs or service text. Keep categories.
    match error {
        ureq::Error::Timeout(_) => Error::Timeout,
        ureq::Error::Tls(_) | ureq::Error::Rustls(_) => Error::Tls,
        ureq::Error::BodyExceedsLimit(_) => Error::ResponseLimit,
        _ => Error::Network,
    }
}
