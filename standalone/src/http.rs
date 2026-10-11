//! Blocking HTTP runs on service workers, never on the render thread.
use crate::error::{AppError, Result};
use serde::{de::DeserializeOwned, Serialize};
use std::time::Duration;
use ureq::http::{HeaderMap, HeaderName, HeaderValue, Method};
use url::Url;
use zeroize::Zeroizing;

pub const JSON_LIMIT: usize = 8 * 1024 * 1024;
const MAX_REDIRECTS: usize = 5;

pub struct HttpRequest {
    pub method: Method,
    pub url: Url,
    headers: HeaderMap,
    body: Zeroizing<Vec<u8>>,
    pub limit: usize,
    sensitive: bool,
}

impl HttpRequest {
    pub fn get(url: Url) -> Self {
        Self {
            method: Method::GET,
            url,
            headers: HeaderMap::new(),
            body: Zeroizing::new(Vec::new()),
            limit: JSON_LIMIT,
            sensitive: false,
        }
    }

    pub fn json(url: Url, body: &impl Serialize) -> Result<Self> {
        let mut request = Self::get(url);
        request.method = Method::POST;
        request.body =
            Zeroizing::new(serde_json::to_vec(body).map_err(|_| AppError::InvalidRequest)?);
        request.header("content-type", "application/json")?;
        Ok(request)
    }

    pub fn header(&mut self, name: &str, value: &str) -> Result<()> {
        let name = HeaderName::from_bytes(name.as_bytes()).map_err(|_| AppError::InvalidRequest)?;
        let mut value = HeaderValue::from_str(value).map_err(|_| AppError::InvalidRequest)?;
        let sensitive = !matches!(
            name.as_str(),
            "accept-language" | "content-type" | "accept" | "user-agent"
        );
        self.sensitive |= sensitive;
        value.set_sensitive(sensitive);
        self.headers.insert(name, value);
        Ok(())
    }

    pub fn header_value(&self, name: &str) -> Option<&str> {
        self.headers.get(name).and_then(|value| value.to_str().ok())
    }

    pub fn body_bytes(&self) -> &[u8] {
        &self.body
    }
}

pub struct HttpResponse {
    pub status: u16,
    pub body: Zeroizing<Vec<u8>>,
}

impl HttpResponse {
    pub fn success(&self) -> Result<()> {
        if (200..300).contains(&self.status) {
            Ok(())
        } else {
            Err(AppError::Http(self.status))
        }
    }

    pub fn json<T: DeserializeOwned>(&self) -> Result<T> {
        self.success()?;
        serde_json::from_slice(&self.body).map_err(|_| AppError::InvalidResponse)
    }
}

pub trait Transport: Send + Sync {
    fn send(&self, request: HttpRequest) -> Result<HttpResponse>;
}

#[derive(Clone)]
pub struct HttpClient {
    agent: ureq::Agent,
}

impl Default for HttpClient {
    fn default() -> Self {
        Self::with_timeout(Duration::from_secs(25))
    }
}

impl HttpClient {
    pub fn with_timeout(timeout: Duration) -> Self {
        let config = ureq::Agent::config_builder()
            .timeout_global(Some(timeout))
            .http_status_as_error(false)
            .max_redirects(0)
            .build();
        Self {
            agent: config.into(),
        }
    }
}

pub fn parse_service_url(value: &str) -> Result<Url> {
    let mut url = Url::parse(value).map_err(|_| AppError::InvalidUrl)?;
    if !matches!(url.scheme(), "http" | "https")
        || url.host_str().is_none()
        || !url.username().is_empty()
        || url.password().is_some()
    {
        return Err(AppError::InvalidUrl);
    }
    url.set_fragment(None);
    Ok(url)
}

fn transport_error(error: ureq::Error) -> AppError {
    match error {
        ureq::Error::Timeout(_) => AppError::Timeout,
        ureq::Error::BodyExceedsLimit(_) => AppError::ResponseTooLarge,
        ureq::Error::Tls(_) | ureq::Error::Rustls(_) | ureq::Error::TlsRequired => AppError::Tls,
        _ => AppError::Connection,
    }
}

impl Transport for HttpClient {
    fn send(&self, request: HttpRequest) -> Result<HttpResponse> {
        let mut url = parse_service_url(request.url.as_str())?;
        let started = std::time::Instant::now();
        for redirects in 0..=MAX_REDIRECTS {
            // One budget covers the entire redirect chain, including body reads.
            let budget = self
                .agent
                .config()
                .timeouts()
                .global
                .unwrap_or(Duration::from_secs(25));
            let remaining = budget
                .checked_sub(started.elapsed())
                .ok_or(AppError::Timeout)?;
            let mut raw = ureq::http::Request::builder()
                .method(request.method.clone())
                .uri(url.as_str())
                .header(
                    "user-agent",
                    concat!("NuvioRust/", env!("CARGO_PKG_VERSION")),
                )
                .header("accept", "application/json, */*");
            for (name, value) in &request.headers {
                raw = raw.header(name, value);
            }
            let raw = raw
                .body(request.body.as_slice())
                .map_err(|_| AppError::InvalidRequest)?;
            // ureq's request-specific configuration also bounds response reads.
            let raw = self
                .agent
                .configure_request(raw)
                .timeout_global(Some(remaining))
                .build();
            let mut response = self.agent.run(raw).map_err(transport_error)?;
            let status = response.status().as_u16();
            if matches!(status, 301 | 302 | 303 | 307 | 308) {
                if redirects == MAX_REDIRECTS {
                    return Err(AppError::TooManyRedirects);
                }
                let location = response
                    .headers()
                    .get("location")
                    .and_then(|v| v.to_str().ok())
                    .ok_or(AppError::InvalidResponse)?;
                let next = parse_service_url(
                    url.join(location)
                        .map_err(|_| AppError::InvalidUrl)?
                        .as_str(),
                )?;
                if (url.scheme() == "https" && next.scheme() != "https")
                    || (request.sensitive && url.origin() != next.origin())
                    || (request.method != Method::GET && !matches!(status, 307 | 308))
                    || (!request.body.is_empty() && url.origin() != next.origin())
                {
                    return Err(AppError::UnsafeRedirect);
                }
                url = next;
                continue;
            }
            let bytes = response
                .body_mut()
                .with_config()
                .limit(request.limit as u64)
                .read_to_vec()
                .map_err(transport_error)?;
            return Ok(HttpResponse {
                status,
                body: Zeroizing::new(bytes),
            });
        }
        Err(AppError::TooManyRedirects)
    }
}
