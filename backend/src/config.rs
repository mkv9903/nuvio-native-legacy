use crate::{
    error::{Error, Result},
    model::Secret,
};
use url::Url;

pub mod built {
    include!(concat!(env!("OUT_DIR"), "/service_config.rs"));
}

#[derive(Clone)]
pub struct AccountConfig {
    pub base: Url,
    pub public_key: Secret,
    pub login_base: Url,
}
impl AccountConfig {
    pub fn built() -> Result<Self> {
        Self::new(
            built::NUVIO_SUPABASE_URL,
            built::NUVIO_SUPABASE_ANON_KEY,
            built::TV_LOGIN_WEB_BASE_URL,
        )
    }
    pub fn new(base: &str, public_key: &str, login_base: &str) -> Result<Self> {
        let mut base = parse_url(base)?;
        let login_base = parse_url(login_base)?;
        if public_key.is_empty() || public_key.chars().any(char::is_control) {
            return Err(Error::Configuration);
        }
        base.set_path(&format!("{}/", base.path().trim_end_matches('/')));
        Ok(Self {
            base,
            public_key: Secret::new(public_key),
            login_base,
        })
    }
    /// A diagnostic comparison for C capabilities awaiting migration. Only
    /// equality is exposed; keys and service URLs never cross back to the UI.
    pub fn matches_compiled(base: &str, public_key: &str, login_base: &str) -> bool {
        Self::new(base, public_key, login_base)
            .ok()
            .zip(Self::built().ok())
            .is_some_and(|(other, built)| {
                other.base == built.base
                    && other.login_base == built.login_base
                    && other.public_key.expose() == built.public_key.expose()
            })
    }
}

fn parse_url(value: &str) -> Result<Url> {
    let url = Url::parse(value).map_err(|_| Error::Configuration)?;
    let local = matches!(url.host_str(), Some("127.0.0.1" | "localhost" | "[::1]"));
    if (url.scheme() != "https" && !(url.scheme() == "http" && local))
        || url.host_str().is_none()
        || !url.username().is_empty()
        || url.password().is_some()
        || url.query().is_some()
        || url.fragment().is_some()
    {
        return Err(Error::Configuration);
    }
    Ok(url)
}
