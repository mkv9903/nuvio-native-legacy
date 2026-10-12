use nuvio_backend::{
    addons::{AddonUrl, InstalledAddon, Manifest, ResourceKind},
    error::Error,
};
use serde_json::json;

fn manifest(value: serde_json::Value) -> Manifest {
    Manifest::decode(&serde_json::to_vec(&value).unwrap()).unwrap()
}

#[test]
fn provider_urls_normalize_manifest_paths_without_losing_configuration() {
    for input in [
        "https://addon.invalid/config",
        "https://addon.invalid/config/",
        " https://addon.invalid/config/Manifest.json/ \n",
        "stremio://addon.invalid/config/manifest.json",
    ] {
        assert_eq!(
            AddonUrl::parse(input).unwrap().as_str(),
            "https://addon.invalid/config"
        );
    }
    let configured =
        AddonUrl::parse("https://addon.invalid/config/manifest.json?token=A%2FB&format=x#fragment")
            .unwrap();
    assert_eq!(
        configured.as_str(),
        "https://addon.invalid/config?token=A%2FB&format=x"
    );
    assert_eq!(
        configured.manifest_url().as_str(),
        "https://addon.invalid/config/manifest.json?token=A%2FB&format=x"
    );
    assert_eq!(
        configured
            .resource_url("catalog", "movie", "top")
            .unwrap()
            .as_str(),
        "https://addon.invalid/config/catalog/movie/top.json?token=A%2FB&format=x"
    );
    let root = AddonUrl::parse("https://addon.invalid/?token=keep/").unwrap();
    assert_eq!(root.as_str(), "https://addon.invalid?token=keep/");
    assert_eq!(root.manifest_url().query(), Some("token=keep/"));
    assert_eq!(
        AddonUrl::parse("https://addon.invalid/?").unwrap().as_str(),
        "https://addon.invalid"
    );
}

#[test]
fn requests_encode_title_path_segments_and_keep_complete_long_urls() {
    let config = "sensitive-config".repeat(500);
    let base = AddonUrl::parse(&format!(
        "https://addon.invalid/{config}/manifest.json?key=value"
    ))
    .unwrap();
    let url = base
        .resource_url("stream", "series", "kitsu:123:1:2 / 日?#%")
        .unwrap();
    assert!(url.as_str().contains(&config));
    assert!(url.as_str().contains("%2F"));
    assert!(url.as_str().contains("%20"));
    assert!(url.as_str().contains("%E6%97%A5"));
    assert!(url.as_str().contains("%3F%23%25.json"));
    assert_eq!(url.query(), Some("key=value"));
    assert!(url.fragment().is_none());
    assert_eq!(
        base.resource_url("stream", "movie", ""),
        Err(Error::InvalidData)
    );
    assert_eq!(
        base.resource_url("stream", "movie", "bad\0id"),
        Err(Error::InvalidData)
    );
}

#[test]
fn invalid_urls_are_rejected_and_debug_output_never_exposes_credentials() {
    for input in [
        "",
        "no-scheme",
        "file:///local",
        "ftp://addon.invalid",
        "https://",
        "https://addon.invalid/config\nsecret",
    ] {
        assert!(AddonUrl::parse(input).is_err(), "invalid URL accepted");
    }
    let url =
        AddonUrl::parse("https://user:password@addon.invalid/private-key?query=secret").unwrap();
    let debug = format!("{url:?}");
    for secret in ["user", "password", "private-key", "query", "secret"] {
        assert!(!debug.contains(secret));
    }
    let restored: AddonUrl = serde_json::from_slice(&serde_json::to_vec(&url).unwrap()).unwrap();
    assert_eq!(restored, url);
    assert!(serde_json::from_str::<AddonUrl>("\"file:///private\"").is_err());
}

#[test]
fn account_addon_lists_have_no_old_count_or_string_caps() {
    let entries: Vec<_> = (0..80).map(|index| json!({"url":format!("https://addon{index}.invalid/{}/manifest.json", "config".repeat(500)), "name":format!("{} {index}", "display-name".repeat(30)), "enabled":index!=79})).collect();
    let list = InstalledAddon::decode_account_list(&serde_json::to_vec(&entries).unwrap()).unwrap();
    assert_eq!(list.len(), 80);
    assert!(list[79].url.as_str().len() > 2047);
    assert!(list[79].name.len() > 64);
    assert!(!list[79].enabled);
    assert!(list[78].enabled);
}

#[test]
fn empty_is_authoritative_and_malformed_entries_reject_the_entire_list() {
    assert!(InstalledAddon::decode_account_list(b"[]")
        .unwrap()
        .is_empty());
    let defaults = InstalledAddon::decode_account_list(br#"[{"url":"https://addon.invalid"},{"url":"https://second.invalid","name":null,"enabled":null}]"#).unwrap();
    assert!(defaults
        .iter()
        .all(|addon| addon.enabled && addon.name == "Addon"));
    for value in [
        json!(null),
        json!({"addons":[]}),
        json!([{"url":"https://valid.invalid"},{}]),
        json!([{"url":"https://valid.invalid"},{"url":""}]),
        json!([{"url":"https://valid.invalid","enabled":"false"}]),
        json!([{"url":"https://valid.invalid","name":false}]),
    ] {
        assert_eq!(
            InstalledAddon::decode_account_list(&serde_json::to_vec(&value).unwrap()),
            Err(Error::InvalidData)
        );
    }
    assert_eq!(
        InstalledAddon::decode_account_list(&vec![b' '; 4 * 1024 * 1024 + 1]),
        Err(Error::ResponseLimit)
    );
}

#[test]
fn manifest_root_identity_and_name_cannot_be_taken_from_nested_catalogs() {
    let result = manifest(
        json!({"catalogs":[{"id":"nested-catalog","type":"movie","name":"Nested name"}], "id":"root-addon", "name":"Root name", "resources":["catalog","stream","subtitles"]}),
    );
    assert_eq!(result.id, "root-addon");
    assert_eq!(result.name, "Root name");
    assert_eq!(result.provides(ResourceKind::Catalog), Some(true));
    assert_eq!(result.provides(ResourceKind::Stream), Some(true));
    assert_eq!(result.provides(ResourceKind::Subtitles), Some(true));
    assert_eq!(result.provides(ResourceKind::Metadata), Some(false));
    assert_eq!(
        result.catalogs[0].key(&result.id),
        "root-addon_movie_nested-catalog"
    );
}

#[test]
fn resource_capabilities_match_names_not_arbitrary_nested_strings() {
    let result = manifest(
        json!({"resources":["Stream",{"name":"Catalog","types":["movie"],"idPrefixes":["stream","meta","subtitles"]}]}),
    );
    assert_eq!(result.provides(ResourceKind::Stream), Some(true));
    assert_eq!(result.provides(ResourceKind::Catalog), Some(true));
    assert_eq!(result.provides(ResourceKind::Metadata), Some(false));
    assert_eq!(result.provides(ResourceKind::Subtitles), Some(false));
    let absent = manifest(json!({"catalogs":[]}));
    assert_eq!(absent.provides(ResourceKind::Stream), None);
    assert_eq!(
        absent.accepts(ResourceKind::Metadata, "movie", "tt123"),
        None
    );
    let explicit_empty = manifest(json!({"resources":[]}));
    assert_eq!(explicit_empty.provides(ResourceKind::Stream), Some(false));
}

#[test]
fn string_resources_inherit_root_types_and_prefixes() {
    let result = manifest(
        json!({"resources":["meta"],"types":["movie","SERIES"],"idPrefixes":["tt","tmdb:"]}),
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", "tt123"),
        Some(true)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", "kitsu:123"),
        Some(false)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "channel", "tt123"),
        Some(false)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "movie", "tmdb:123"),
        Some(true)
    );
}

#[test]
fn standalone_resource_omitted_prefixes_accept_all_ids_as_specified() {
    let result = manifest(
        json!({"resources":[{"name":"meta","types":["series"]}],"types":["movie"],"idPrefixes":["tt"]}),
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", "kitsu:123"),
        Some(true)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", "tt123"),
        Some(true)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "movie", "tt123"),
        Some(false)
    );
    let filtered = manifest(
        json!({"resources":[{"name":"meta","types":["series"],"idPrefixes":["kitsu:"]}],"idPrefixes":["tt"]}),
    );
    assert_eq!(
        filtered.accepts(ResourceKind::Metadata, "series", "tt123"),
        Some(false)
    );
    assert_eq!(
        filtered.accepts(ResourceKind::Metadata, "series", "kitsu:123"),
        Some(true)
    );
}

#[test]
fn resource_filters_keep_prefixes_beyond_old_fixed_arrays() {
    let prefix = "long-prefix-".repeat(30);
    let mut prefixes: Vec<_> = (0..40).map(|index| format!("prefix{index}:")).collect();
    prefixes.push(prefix.clone());
    let result =
        manifest(json!({"resources":[{"name":"meta","types":["series"],"idPrefixes":prefixes}]}));
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", &format!("{prefix}123")),
        Some(true)
    );
    assert_eq!(
        result.accepts(ResourceKind::Metadata, "series", "prefix39:123"),
        Some(true)
    );
}

#[test]
fn search_catalogs_beyond_home_quota_and_required_search_are_distinct() {
    let mut catalogs: Vec<_> = (0..700).map(|index| json!({"id":format!("catalog{index}"),"type":"movie","name":format!("Catalog {index}")})).collect();
    catalogs
        .push(json!({"id":"search","type":"anime","extra":[{"name":"search","isRequired":true}]}));
    catalogs
        .push(json!({"id":"genre","type":"movie","extra":[{"name":"genre","isRequired":true}]}));
    catalogs.push(json!({"id":"legacy","type":"series","extraSupported":["search"]}));
    let result = manifest(json!({"catalogs":catalogs}));
    assert_eq!(result.catalogs.len(), 703);
    assert!(result.catalogs[700].searchable());
    assert!(result.catalogs[700].requires_search());
    assert!(!result.catalogs[701].requires_search());
    assert!(result.catalogs[702].searchable());
    assert!(!result.catalogs[702].requires_search());
}

#[test]
fn channel_catalog_types_are_preserved_for_the_guide() {
    let catalogs: Vec<_> = ["tv", "channel", "channels", "live", "iptv", "movie"]
        .iter()
        .enumerate()
        .map(|(index, kind)| json!({"id":format!("c{index}"),"type":kind}))
        .collect();
    let result = manifest(json!({"catalogs":catalogs}));
    assert!(result.catalogs[..5]
        .iter()
        .all(|catalog| catalog.is_channel()));
    assert!(!result.catalogs[5].is_channel());
}

#[test]
fn malformed_manifests_and_oversized_replies_are_rejected() {
    for body in [
        "[]",
        "null",
        "",
        "{",
        "{\"resources\":true}",
        "{\"resources\":[{}]}",
        "{\"catalogs\":[{\"id\":\"\",\"type\":\"movie\"}]}",
        "{\"catalogs\":[{\"id\":\"a\\u0000b\",\"type\":\"movie\"}]}",
    ] {
        assert!(Manifest::decode(body.as_bytes()).is_err());
    }
    assert_eq!(
        Manifest::decode(&vec![b' '; 4 * 1024 * 1024 + 1]),
        Err(Error::ResponseLimit)
    );
}
