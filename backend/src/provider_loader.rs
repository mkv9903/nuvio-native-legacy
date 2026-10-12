//! Cached provider loading with account/profile guards around every I/O phase.
use crate::{
    addon_service::AddonAccount,
    addons::{AddonUrl, Manifest},
    catalog_cache::{ProviderCache, ProviderKey, ProviderResource},
    catalog_titles::CatalogReply,
    content_context::{ContentContext, ContentScope},
    error::{Error, Result},
    provider_http::ProviderTransport,
};
use std::{sync::Arc, time::Duration};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum LoadOrigin {
    Network,
    Cache,
}
#[derive(Clone, Debug)]
pub struct Loaded<T> {
    pub value: T,
    pub origin: LoadOrigin,
    pub request_error: Option<Error>,
    pub cache_error: Option<Error>,
}
#[derive(Clone)]
pub struct ProviderLoader {
    account: Arc<dyn AddonAccount>,
    transport: Arc<dyn ProviderTransport>,
    cache: Arc<dyn ProviderCache>,
}
impl ProviderLoader {
    pub fn new(
        account: Arc<dyn AddonAccount>,
        transport: Arc<dyn ProviderTransport>,
        cache: Arc<dyn ProviderCache>,
    ) -> Self {
        Self {
            account,
            transport,
            cache,
        }
    }
    fn current(&self, context: &ContentContext) -> Result<()> {
        if self.account.is_current(context) {
            Ok(())
        } else {
            Err(Error::Cancelled)
        }
    }
    pub fn key(
        context: &ContentContext,
        addon: &AddonUrl,
        resource: ProviderResource,
    ) -> ProviderKey {
        ProviderKey {
            scope: ContentScope::from(context),
            addon: addon.clone(),
            resource,
        }
    }
    fn cached<T>(
        &self,
        context: &ContentContext,
        key: &ProviderKey,
        parse: &impl Fn(&[u8]) -> Result<T>,
    ) -> Result<Option<T>> {
        self.current(context)?;
        let cached = self.cache.load(key);
        self.current(context)?;
        cached?.map(|body| parse(&body)).transpose()
    }
    fn load<T>(
        &self,
        context: &ContentContext,
        key: &ProviderKey,
        timeout: Duration,
        parse: impl Fn(&[u8]) -> Result<T>,
    ) -> Result<Loaded<T>> {
        let (cached, cache_error) = match self.cached(context, key, &parse) {
            Ok(value) => (value, None),
            Err(Error::Cancelled) => return Err(Error::Cancelled),
            Err(error) => (None, Some(error)),
        };
        self.current(context)?;
        let response = self.transport.get(&key.url()?, timeout);
        self.current(context)?;
        let fetched = response.and_then(|response| {
            response.success()?;
            parse(&response.body).map(|value| (value, response.body))
        });
        self.current(context)?;
        match fetched {
            Ok((value, body)) => {
                let saved = self
                    .cache
                    .save(key, &body, &|| self.account.is_current(context));
                self.current(context)?;
                if saved == Err(Error::Cancelled) {
                    return Err(Error::Cancelled);
                }
                Ok(Loaded {
                    value,
                    origin: LoadOrigin::Network,
                    request_error: None,
                    cache_error: saved.err(),
                })
            }
            Err(error) => cached
                .map(|value| Loaded {
                    value,
                    origin: LoadOrigin::Cache,
                    request_error: Some(error.clone()),
                    cache_error,
                })
                .ok_or(error),
        }
    }
    pub fn cached_manifest(
        &self,
        context: &ContentContext,
        addon: &AddonUrl,
    ) -> Result<Option<Manifest>> {
        self.cached(
            context,
            &Self::key(context, addon, ProviderResource::Manifest),
            &Manifest::decode,
        )
    }
    pub fn manifest(&self, context: &ContentContext, addon: &AddonUrl) -> Result<Loaded<Manifest>> {
        self.load(
            context,
            &Self::key(context, addon, ProviderResource::Manifest),
            Duration::from_secs(12),
            Manifest::decode,
        )
    }
    pub fn cached_catalog(
        &self,
        context: &ContentContext,
        addon: &AddonUrl,
        resource: ProviderResource,
        quota: Option<usize>,
    ) -> Result<Option<CatalogReply>> {
        let ProviderResource::Catalog { media_type, .. } = &resource else {
            return Err(Error::InvalidData);
        };
        let media_type = media_type.clone();
        self.cached(context, &Self::key(context, addon, resource), &|body| {
            CatalogReply::decode_with_limit(body, &media_type, quota)
        })
    }
    pub fn catalog(
        &self,
        context: &ContentContext,
        addon: &AddonUrl,
        resource: ProviderResource,
        quota: Option<usize>,
    ) -> Result<Loaded<CatalogReply>> {
        let ProviderResource::Catalog { media_type, .. } = &resource else {
            return Err(Error::InvalidData);
        };
        let media_type = media_type.clone();
        self.load(
            context,
            &Self::key(context, addon, resource),
            Duration::from_secs(8),
            |body| CatalogReply::decode_with_limit(body, &media_type, quota),
        )
    }
}
