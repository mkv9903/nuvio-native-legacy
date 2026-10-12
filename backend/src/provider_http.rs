//! Unauthenticated addon traffic, isolated from the account HTTP client.
use crate::{
    addons::ADDON_RESPONSE_LIMIT,
    error::{Error, Result},
    http::{map_transport_error, Response},
};
use std::time::Duration;
use url::Url;
use zeroize::Zeroizing;

pub trait ProviderTransport: Send + Sync {
    fn get(&self, url: &Url, timeout: Duration) -> Result<Response>;
}
pub struct ProviderHttp {
    agent: ureq::Agent,
}
impl Default for ProviderHttp {
    fn default() -> Self {
        Self {
            agent: ureq::Agent::config_builder()
                .timeout_global(Some(Duration::from_secs(12)))
                .timeout_connect(Some(Duration::from_secs(8)))
                .http_status_as_error(false)
                .max_redirects(5)
                .redirect_auth_headers(ureq::config::RedirectAuthHeaders::Never)
                .max_idle_connections(4)
                .max_idle_connections_per_host(2)
                .user_agent("Nuvio Rust Test")
                .build()
                .new_agent(),
        }
    }
}
impl ProviderTransport for ProviderHttp {
    fn get(&self, url: &Url, timeout: Duration) -> Result<Response> {
        if !matches!(url.scheme(), "http" | "https")
            || url.host_str().is_none()
            || timeout.is_zero()
        {
            return Err(Error::InvalidData);
        }
        // No account session/key can be passed to this transport. Redirected
        // requests also receive no account headers, cookies or request bodies.
        let mut response = self
            .agent
            .get(url.as_str())
            .header("Accept", "application/json")
            .config()
            .timeout_global(Some(timeout))
            .build()
            .call()
            .map_err(map_transport_error)?;
        let status = response.status().as_u16();
        let body = response
            .body_mut()
            .with_config()
            .limit(ADDON_RESPONSE_LIMIT as u64)
            .read_to_vec()
            .map_err(map_transport_error)?;
        Ok(Response {
            status,
            body: Zeroizing::new(body),
        })
    }
}
