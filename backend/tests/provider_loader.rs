mod support;
use nuvio_backend::{
    addon_service::AddonAccount,
    addons::{AddonUrl, InstalledAddon},
    catalog_cache::{CatalogCache, ProviderCache, ProviderResource},
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
    http::Response,
    model::ProfileId,
    provider_http::ProviderTransport,
    provider_loader::{LoadOrigin, ProviderLoader},
};
use serde_json::json;
use std::{
    fs,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Barrier, Mutex,
    },
    time::Duration,
};
use support::Directory;
use url::Url;
use zeroize::Zeroizing;

fn context(generation: u64) -> ContentContext {
    ContentContext {
        account_generation: 1,
        content_generation: generation,
        profile: ProfileId::new(2).unwrap(),
        addons: AddonScope {
            member: "member".into(),
            owner: "owner".into(),
            profile: ProfileId::new(1).unwrap(),
        },
        force_primary: false,
    }
}
struct Account(Mutex<ContentContext>);
impl AddonAccount for Account {
    fn current_context(&self, _: bool) -> Option<ContentContext> {
        Some(self.0.lock().unwrap().clone())
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        &*self.0.lock().unwrap() == context
    }
    fn read_addons(&self, _: &ContentContext) -> Result<Vec<InstalledAddon>> {
        unreachable!()
    }
    fn write_addons(&self, _: &ContentContext, _: &[InstalledAddon]) -> Result<()> {
        unreachable!()
    }
}
struct Transport {
    action: Box<dyn Fn(&Url, Duration) -> Result<Response> + Send + Sync>,
}
impl ProviderTransport for Transport {
    fn get(&self, url: &Url, timeout: Duration) -> Result<Response> {
        (self.action)(url, timeout)
    }
}
fn provider() -> AddonUrl {
    AddonUrl::parse("https://provider.invalid/config?key=complete").unwrap()
}
fn resource() -> ProviderResource {
    ProviderResource::Catalog {
        media_type: "series".into(),
        id: "row".into(),
        extras: vec![("search".into(), "A & B".into())],
    }
}
fn body(id: &str) -> Vec<u8> {
    serde_json::to_vec(
        &json!({"metas":[{"id":id,"name":"Title","poster":"https://poster.invalid/x.jpg"}]}),
    )
    .unwrap()
}
fn response(body: &[u8]) -> Result<Response> {
    Ok(Response {
        status: 200,
        body: Zeroizing::new(body.to_vec()),
    })
}
fn loader(
    account: Arc<Account>,
    cache: Arc<CatalogCache>,
    callback: impl Fn(&Url, Duration) -> Result<Response> + Send + Sync + 'static,
) -> ProviderLoader {
    ProviderLoader::new(
        account,
        Arc::new(Transport {
            action: Box::new(callback),
        }),
        cache,
    )
}

#[test]
fn cached_titles_are_available_before_network_and_failed_loads_preserve_only_the_same_scope() {
    let directory = Directory::new();
    let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
    let ctx = context(1);
    let key = ProviderLoader::key(&ctx, &provider(), resource());
    cache.save(&key, &body("cached"), &|| true).unwrap();
    let account = Arc::new(Account(Mutex::new(ctx.clone())));
    let calls = Arc::new(AtomicUsize::new(0));
    let count = calls.clone();
    let service = loader(account.clone(), cache.clone(), move |url, timeout| {
        count.fetch_add(1, Ordering::SeqCst);
        assert_eq!(timeout, Duration::from_secs(8));
        assert!(url
            .as_str()
            .contains("/catalog/series/row/search=A%20%26%20B.json?key=complete"));
        Err(Error::Timeout)
    });
    assert_eq!(
        service
            .cached_catalog(&ctx, &provider(), resource(), Some(12))
            .unwrap()
            .unwrap()
            .titles[0]
            .id,
        "cached"
    );
    assert_eq!(calls.load(Ordering::SeqCst), 0);
    let loaded = service
        .catalog(&ctx, &provider(), resource(), Some(12))
        .unwrap();
    assert_eq!(loaded.origin, LoadOrigin::Cache);
    assert_eq!(loaded.request_error, Some(Error::Timeout));
    assert_eq!(loaded.value.titles[0].id, "cached");
    let mut other = ctx.clone();
    other.profile = ProfileId::new(3).unwrap();
    *account.0.lock().unwrap() = other.clone();
    assert_eq!(
        service
            .catalog(&other, &provider(), resource(), None)
            .unwrap_err(),
        Error::Timeout
    );
    assert!(service
        .cached_catalog(&other, &provider(), resource(), None)
        .unwrap()
        .is_none());
}

#[test]
fn wholly_unusable_and_malformed_replies_keep_cache_but_confirmed_empty_replaces_it() {
    for failed in [
        br#"{"metas":[{}]}"#.as_slice(),
        br#"{"metas":null}"#,
        br#"{"metas":[],"error":"failed"}"#,
        b"{",
    ] {
        let directory = Directory::new();
        let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
        let ctx = context(1);
        let key = ProviderLoader::key(&ctx, &provider(), resource());
        let original = body("cached");
        cache.save(&key, &original, &|| true).unwrap();
        let failed = failed.to_vec();
        let service = loader(
            Arc::new(Account(Mutex::new(ctx.clone()))),
            cache.clone(),
            move |_, _| response(&failed),
        );
        let loaded = service
            .catalog(&ctx, &provider(), resource(), None)
            .unwrap();
        assert_eq!(loaded.origin, LoadOrigin::Cache);
        assert_eq!(loaded.value.titles[0].id, "cached");
        assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), original);
    }
    let directory = Directory::new();
    let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
    let ctx = context(1);
    let key = ProviderLoader::key(&ctx, &provider(), resource());
    cache.save(&key, &body("old"), &|| true).unwrap();
    let service = loader(
        Arc::new(Account(Mutex::new(ctx.clone()))),
        cache.clone(),
        |_, _| response(br#"{"metas":[]}"#),
    );
    let loaded = service
        .catalog(&ctx, &provider(), resource(), None)
        .unwrap();
    assert_eq!(loaded.origin, LoadOrigin::Network);
    assert!(loaded.value.titles.is_empty());
    assert_eq!(
        cache.load(&key).unwrap().unwrap().as_slice(),
        br#"{"metas":[]}"#
    );
}

#[test]
fn profile_round_trip_cancels_late_provider_results_before_cache_publication() {
    let directory = Directory::new();
    let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
    let ctx = context(1);
    let account = Arc::new(Account(Mutex::new(ctx.clone())));
    let started = Arc::new(Barrier::new(2));
    let release = Arc::new(Barrier::new(2));
    let ready = started.clone();
    let finish = release.clone();
    let service = loader(account.clone(), cache.clone(), move |_, _| {
        ready.wait();
        finish.wait();
        response(&body("late"))
    });
    std::thread::scope(|threads| {
        let task = threads.spawn(|| service.catalog(&ctx, &provider(), resource(), None));
        started.wait();
        *account.0.lock().unwrap() = context(3);
        release.wait();
        assert_eq!(task.join().unwrap().unwrap_err(), Error::Cancelled);
    });
    assert!(cache
        .load(&ProviderLoader::key(&ctx, &provider(), resource()))
        .unwrap()
        .is_none());
}

#[test]
fn corrupt_cache_does_not_block_a_valid_fetch_and_success_repairs_it() {
    let directory = Directory::new();
    let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
    let ctx = context(1);
    let key = ProviderLoader::key(&ctx, &provider(), resource());
    cache.save(&key, &body("old"), &|| true).unwrap();
    let path = fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| {
            path.file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .starts_with("provider-cache-")
        })
        .unwrap();
    fs::write(&path, "{").unwrap();
    let service = loader(
        Arc::new(Account(Mutex::new(ctx.clone()))),
        cache.clone(),
        |_, _| response(&body("fresh")),
    );
    let loaded = service
        .catalog(&ctx, &provider(), resource(), None)
        .unwrap();
    assert_eq!(loaded.value.titles[0].id, "fresh");
    assert!(loaded.cache_error.is_none());
    assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), body("fresh"));
}

#[test]
fn manifests_have_their_own_request_deadline_and_invalid_resource_kinds_do_no_work() {
    let directory = Directory::new();
    let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
    let ctx = context(1);
    let service = loader(
        Arc::new(Account(Mutex::new(ctx.clone()))),
        cache,
        |url, timeout| {
            assert_eq!(
                url.as_str(),
                "https://provider.invalid/config/manifest.json?key=complete"
            );
            assert_eq!(timeout, Duration::from_secs(12));
            response(br#"{"id":"provider","resources":["stream"],"catalogs":[]}"#)
        },
    );
    let loaded = service.manifest(&ctx, &provider()).unwrap();
    assert_eq!(loaded.value.id, "provider");
    assert_eq!(loaded.origin, LoadOrigin::Network);
    assert_eq!(
        service
            .catalog(&ctx, &provider(), ProviderResource::Manifest, None)
            .unwrap_err(),
        Error::InvalidData
    );
}

#[test]
fn incomplete_manifests_retain_useful_cache_and_explicit_declarations_replace_it() {
    let original = br#"{"id":"provider","resources":["catalog","stream"],"catalogs":[{"id":"top","type":"movie"}]}"#;
    for failed in [
        br#"{"id":"provider","name":"Provider"}"#.as_slice(),
        br#"{}"#,
        br#"{"resources":[],"catalogs":[],"error":"outage"}"#,
        br#"{"catalogs":null}"#,
    ] {
        let directory = Directory::new();
        let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
        let ctx = context(1);
        let key = ProviderLoader::key(&ctx, &provider(), ProviderResource::Manifest);
        cache.save(&key, original, &|| true).unwrap();
        let failed = failed.to_vec();
        let service = loader(
            Arc::new(Account(Mutex::new(ctx.clone()))),
            cache.clone(),
            move |_, _| response(&failed),
        );
        let loaded = service.manifest(&ctx, &provider()).unwrap();
        assert_eq!(loaded.origin, LoadOrigin::Cache);
        assert_eq!(loaded.request_error, Some(Error::InvalidData));
        assert_eq!(loaded.value.catalogs[0].id, "top");
        assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), original);
    }
    for replacement in [
        br#"{"id":"provider","resources":[],"catalogs":[]}"#.as_slice(),
        br#"{"id":"provider","resources":["stream"],"types":["movie"]}"#,
        br#"{"id":"provider","catalogs":[]}"#,
    ] {
        let directory = Directory::new();
        let cache = Arc::new(CatalogCache::open(&directory.path).unwrap());
        let ctx = context(1);
        let key = ProviderLoader::key(&ctx, &provider(), ProviderResource::Manifest);
        cache.save(&key, original, &|| true).unwrap();
        let bytes = replacement.to_vec();
        let service = loader(
            Arc::new(Account(Mutex::new(ctx.clone()))),
            cache.clone(),
            move |_, _| response(&bytes),
        );
        let loaded = service.manifest(&ctx, &provider()).unwrap();
        assert_eq!(loaded.origin, LoadOrigin::Network);
        assert!(loaded.value.catalogs.is_empty());
        assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), replacement);
    }
}
