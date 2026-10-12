//! Versioned original provider documents scoped to the selected Home profile.
use crate::{
    addons::{AddonUrl, Manifest, ADDON_RESPONSE_LIMIT},
    catalog_titles::CatalogReply,
    content_context::ContentScope,
    error::{Error, Result},
    json_store::JsonStore,
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use serde_json::value::RawValue;
use std::{fmt::Write, path::Path};
use url::Url;
use zeroize::Zeroizing;

const CACHE_FILE_LIMIT: u64 = 8 * 1024 * 1024;
#[derive(Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
#[serde(tag = "resource", rename_all = "snake_case")]
pub enum ProviderResource {
    Manifest,
    Catalog {
        media_type: String,
        id: String,
        extras: Vec<(String, String)>,
    },
}
#[derive(Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ProviderKey {
    pub scope: ContentScope,
    pub addon: AddonUrl,
    pub resource: ProviderResource,
}
impl ProviderKey {
    pub fn url(&self) -> Result<Url> {
        if self.scope.member.is_empty() || self.scope.owner.is_empty() {
            return Err(Error::InvalidData);
        }
        match &self.resource {
            ProviderResource::Manifest => Ok(self.addon.manifest_url()),
            ProviderResource::Catalog {
                media_type,
                id,
                extras,
            } => self.addon.resource_url_with_extras(
                "catalog",
                media_type,
                id,
                &extras
                    .iter()
                    .map(|(name, value)| (name.as_str(), value.as_str()))
                    .collect::<Vec<_>>(),
            ),
        }
    }
    fn validate_body(&self, body: &[u8]) -> Result<()> {
        self.url()?;
        match &self.resource {
            ProviderResource::Manifest => {
                Manifest::decode(body)?;
            }
            ProviderResource::Catalog { media_type, .. } => {
                CatalogReply::decode_with_limit(body, media_type, Some(1))?;
            }
        }
        Ok(())
    }
}
pub trait ProviderCache: Send + Sync {
    fn load(&self, key: &ProviderKey) -> Result<Option<Zeroizing<Vec<u8>>>>;
    fn save(&self, key: &ProviderKey, body: &[u8], current: &dyn Fn() -> bool) -> Result<()>;
    fn clear(&self) -> Result<()>;
}
#[derive(Deserialize, Serialize)]
struct Envelope<'a> {
    version: u32,
    app_id: String,
    key: ProviderKey,
    #[serde(borrow)]
    body: &'a RawValue,
}
#[derive(Clone)]
pub struct CatalogCache {
    files: JsonStore,
}
impl CatalogCache {
    pub fn open(directory: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            files: JsonStore::open(directory, TEST_APP_ID)?,
        })
    }
    fn key(key: &ProviderKey) -> Result<String> {
        key.url()?;
        let bytes = Zeroizing::new(serde_json::to_vec(key).map_err(|_| Error::InvalidData)?);
        let hash = ring::digest::digest(&ring::digest::SHA256, &bytes);
        let mut key = String::with_capacity(64);
        for byte in hash.as_ref() {
            write!(key, "{byte:02x}").expect("writing to String");
        }
        Ok(key)
    }
    fn name(key: &ProviderKey) -> Result<String> {
        Ok(format!("provider-cache-{}.json", Self::key(key)?))
    }
}
impl ProviderCache for CatalogCache {
    fn load(&self, key: &ProviderKey) -> Result<Option<Zeroizing<Vec<u8>>>> {
        let Some(bytes) = self.files.read(&Self::name(key)?, CACHE_FILE_LIMIT)? else {
            return Ok(None);
        };
        let envelope: Envelope = serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
        if envelope.version != 1 || envelope.app_id != TEST_APP_ID || &envelope.key != key {
            return Err(Error::InvalidData);
        }
        let body = envelope.body.get().as_bytes();
        key.validate_body(body)?;
        Ok(Some(Zeroizing::new(body.to_vec())))
    }
    fn save(&self, key: &ProviderKey, body: &[u8], current: &dyn Fn() -> bool) -> Result<()> {
        if body.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        key.validate_body(body)?;
        let body: &RawValue = serde_json::from_slice(body).map_err(|_| Error::InvalidData)?;
        let envelope = Envelope {
            version: 1,
            app_id: TEST_APP_ID.into(),
            key: key.clone(),
            body,
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&envelope).map_err(|_| Error::InvalidData)?);
        self.files
            .with_lock(&format!("provider-lock-{}", Self::key(key)?), || {
                if !current() {
                    return Err(Error::Cancelled);
                }
                self.files
                    .write(&Self::name(key)?, &bytes, CACHE_FILE_LIMIT)
            })
    }
    fn clear(&self) -> Result<()> {
        for name in self.files.names()? {
            if let Some(key) = name
                .strip_prefix("provider-cache-")
                .and_then(|value| value.strip_suffix(".json"))
                .filter(|key| key.len() == 64 && key.bytes().all(|byte| byte.is_ascii_hexdigit()))
            {
                self.files
                    .with_lock(&format!("provider-lock-{key}"), || self.files.remove(&name))?;
            }
        }
        Ok(())
    }
}
