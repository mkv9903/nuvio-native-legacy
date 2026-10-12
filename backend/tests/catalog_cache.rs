mod support;
use nuvio_backend::{
    addons::AddonUrl,
    catalog_cache::{CatalogCache, ProviderCache, ProviderKey, ProviderResource},
    content_context::{AddonScope, ContentContext, ContentScope},
    error::Error,
    model::ProfileId,
};
use serde_json::json;
use std::fs;
use support::Directory;

fn key(profile: u32) -> ProviderKey {
    ProviderKey {
        scope: ContentScope {
            member: "member".into(),
            owner: "shared-owner".into(),
            profile: ProfileId::new(profile).unwrap(),
        },
        addon: AddonUrl::parse("https://provider.invalid/config?private=keep").unwrap(),
        resource: ProviderResource::Catalog {
            media_type: "movie".into(),
            id: "row".into(),
            extras: vec![],
        },
    }
}
fn body(id: &str) -> Vec<u8> {
    serde_json::to_vec(&json!({"metas":[{"id":id,"name":"Title","poster":"https://poster.invalid/x.jpg"}], "cacheMaxAge":300})).unwrap()
}
fn file(directory: &Directory) -> std::path::PathBuf {
    fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| {
            path.file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .starts_with("provider-cache-")
        })
        .unwrap()
}

#[test]
fn full_documents_round_trip_with_private_hashed_names_and_no_legacy_imports() {
    let directory = Directory::new();
    let cache = CatalogCache::open(&directory.path).unwrap();
    let key = key(2);
    fs::write(directory.path.join("catalogo-rede.bin"), "legacy snapshot").unwrap();
    assert!(cache.load(&key).unwrap().is_none());
    let body = body(&format!("provider:{}", "long-id".repeat(1200)));
    cache.save(&key, &body, &|| true).unwrap();
    assert_eq!(
        CatalogCache::open(&directory.path)
            .unwrap()
            .load(&key)
            .unwrap()
            .unwrap()
            .as_slice(),
        body
    );
    let path = file(&directory);
    let name = path.file_name().unwrap().to_str().unwrap();
    assert!(!name.contains("private"));
    assert!(!name.contains("member"));
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(path).unwrap().permissions().mode() & 0o777,
            0o600
        );
    }
    cache.clear().unwrap();
    assert!(cache.load(&key).unwrap().is_none());
    assert_eq!(
        fs::read_to_string(directory.path.join("catalogo-rede.bin")).unwrap(),
        "legacy snapshot"
    );
}

#[test]
fn cache_ownership_includes_selected_profile_owner_member_configuration_and_extras() {
    let directory = Directory::new();
    let cache = CatalogCache::open(&directory.path).unwrap();
    let original = key(2);
    cache.save(&original, &body("original"), &|| true).unwrap();
    let mut variants = vec![key(3)];
    let mut changed = original.clone();
    changed.scope.member = "other-member".into();
    variants.push(changed);
    let mut changed = original.clone();
    changed.scope.owner = "other-owner".into();
    variants.push(changed);
    let mut changed = original.clone();
    changed.addon = AddonUrl::parse("https://provider.invalid/config?private=different").unwrap();
    variants.push(changed);
    let mut changed = original.clone();
    changed.resource = ProviderResource::Catalog {
        media_type: "movie".into(),
        id: "row".into(),
        extras: vec![("skip".into(), "100".into())],
    };
    variants.push(changed);
    for variant in variants {
        assert!(cache.load(&variant).unwrap().is_none());
    }
    let context = ContentContext {
        account_generation: 1,
        content_generation: 3,
        profile: ProfileId::new(2).unwrap(),
        addons: AddonScope {
            member: "member".into(),
            owner: "shared-owner".into(),
            profile: ProfileId::new(1).unwrap(),
        },
        force_primary: false,
    };
    assert_eq!(ContentScope::from(&context).profile.get(), 2);
}

#[test]
fn confirmed_empty_replaces_old_titles_but_invalid_or_cancelled_writes_keep_them() {
    let directory = Directory::new();
    let cache = CatalogCache::open(&directory.path).unwrap();
    let key = key(1);
    let original = body("old");
    cache.save(&key, &original, &|| true).unwrap();
    for invalid in [
        r#"{"metas":[{}]}"#,
        r#"{"metas":null}"#,
        r#"{"error":"failed"}"#,
        "{",
    ] {
        assert_eq!(
            cache.save(&key, invalid.as_bytes(), &|| true),
            Err(Error::InvalidData)
        );
        assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), original);
    }
    assert_eq!(
        cache.save(&key, &body("late"), &|| false),
        Err(Error::Cancelled)
    );
    assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), original);
    cache.save(&key, br#"{"metas":[]}"#, &|| true).unwrap();
    assert_eq!(
        cache.load(&key).unwrap().unwrap().as_slice(),
        br#"{"metas":[]}"#
    );
}

#[test]
fn unknown_versions_corrupt_identity_and_symlinks_are_rejected_without_removing_files() {
    let directory = Directory::new();
    let cache = CatalogCache::open(&directory.path).unwrap();
    let key = key(1);
    cache.save(&key, &body("original"), &|| true).unwrap();
    let path = file(&directory);
    let original: serde_json::Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
    for (field, value) in [
        ("version", json!(99)),
        ("app_id", json!("production")),
        ("key", json!({})),
    ] {
        let mut wrong = original.clone();
        wrong[field] = value;
        let bytes = serde_json::to_vec(&wrong).unwrap();
        fs::write(&path, &bytes).unwrap();
        assert_eq!(cache.load(&key).unwrap_err(), Error::InvalidData);
        assert_eq!(fs::read(&path).unwrap(), bytes);
    }
    #[cfg(unix)]
    {
        use std::os::unix::fs::symlink;
        fs::remove_file(&path).unwrap();
        let target = directory.parent.join("outside.json");
        fs::write(&target, "private").unwrap();
        symlink(&target, &path).unwrap();
        assert_eq!(cache.load(&key).unwrap_err(), Error::Storage);
        assert_eq!(fs::read_to_string(target).unwrap(), "private");
    }
}

#[test]
fn manifest_documents_use_distinct_keys_and_long_provider_configuration_is_preserved() {
    let directory = Directory::new();
    let cache = CatalogCache::open(&directory.path).unwrap();
    let mut key = key(1);
    key.addon = AddonUrl::parse(&format!(
        "https://provider.invalid/{}?private=keep",
        "config".repeat(1000)
    ))
    .unwrap();
    key.resource = ProviderResource::Manifest;
    let body = br#"{"id":"provider","name":"Provider","resources":["stream"],"catalogs":[]}"#;
    cache.save(&key, body, &|| true).unwrap();
    assert_eq!(cache.load(&key).unwrap().unwrap().as_slice(), body);
    let mut catalog = key.clone();
    catalog.resource = ProviderResource::Catalog {
        media_type: "movie".into(),
        id: "row".into(),
        extras: vec![],
    };
    assert!(cache.load(&catalog).unwrap().is_none());
    assert!(key
        .url()
        .unwrap()
        .as_str()
        .ends_with("/manifest.json?private=keep"));
}
