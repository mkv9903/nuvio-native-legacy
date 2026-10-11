mod support;
use nuvio_rust_app::{
    account::{BackendConfig, CloudClient, Session, SessionVault},
    catalog::{
        addon_endpoint, home_order, installed_addons, load_profiles, Addon, CatalogService,
        HomeOrder, InstalledAddon, Source, Title,
    },
    profile::{Profile, ProfileHome},
    secret::Secret,
};
use serde_json::{json, Value};
use std::sync::{Arc, Mutex};
use support::{fake, response, FixtureDirectory};

fn addon(id: &str, catalogs: Value) -> Addon {
    Addon { record: Arc::new(InstalledAddon { url: format!("https://{id}.example.invalid/private/manifest.json?key=fixture"), name: id.to_owned(), enabled: true }),
        manifest: Arc::new(serde_json::from_value(json!({"id":id,"name":id,"types":["movie","series"],"resources":["catalog","meta","stream"],"catalogs":catalogs})).unwrap()) }
}

#[test]
fn shared_addons_use_primary_profile_but_home_order_uses_active_profile() {
    let directory = FixtureDirectory::new();
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let generation = vault
        .install(
            vault.generation(),
            Session {
                access_token: Secret::new("fixture-access"),
                refresh_token: Secret::new("fixture-refresh"),
                expires_at: u64::MAX,
                user_id: "fixture-viewer".to_owned(),
            },
        )
        .unwrap();
    let requests = Arc::new(Mutex::new(Vec::new()));
    let seen = requests.clone();
    let transport = fake(move |request| {
        let path = request.url.path().to_owned();
        let query: Vec<_> = request
            .url
            .query_pairs()
            .map(|(key, value)| (key.into_owned(), value.into_owned()))
            .collect();
        let body: Value = serde_json::from_slice(request.body_bytes()).unwrap_or(Value::Null);
        seen.lock().unwrap().push(path.clone());
        let result = match path.as_str() {
            "/rest/v1/rpc/get_sync_owner" => json!("fixture-owner"),
            "/rest/v1/rpc/sync_pull_profiles" => json!([
                {"profile_index":1,"name":"Primary","is_primary":true},
                {"profile_index":2,"name":"Shared","uses_primary_addons":true},
                {"profile_index":3,"name":"Independent"}]),
            "/rest/v1/rpc/sync_pull_profile_locks" => {
                json!([{ "profile_id":3,"pin_enabled":true }])
            }
            "/rest/v1/addons" => {
                assert!(query.contains(&("user_id".to_owned(), "eq.fixture-owner".to_owned())));
                assert!(query.contains(&("profile_id".to_owned(), "eq.1".to_owned())));
                json!([{ "url":"https://addon.example.invalid/manifest.json","name":"Fixture","enabled":true }])
            }
            "/rest/v1/rpc/sync_pull_home_catalog_settings" => {
                assert_eq!(body["p_profile_id"], 2);
                assert_eq!(body["p_platform"], "home_catalog_shared");
                json!([{ "settings_json":{"items":[]} }])
            }
            _ => panic!("unexpected fixture endpoint"),
        };
        Ok(response(200, result))
    });
    let config = Arc::new(
        BackendConfig::new(
            "https://backend.example.invalid",
            "fixture-anon",
            "https://login.example.invalid/tv",
        )
        .unwrap(),
    );
    let cloud = CloudClient::new(config, transport, vault);
    let account = load_profiles(&cloud, generation).unwrap();
    assert!(account.profiles[2].has_pin);
    let mut home = ProfileHome::<String>::default();
    let old = home
        .begin(&account.owner, &account.profiles[0], &account.profiles)
        .unwrap();
    assert!(home.replace(&old, vec!["old row".to_owned()]));
    let shared = home
        .begin(&account.owner, &account.profiles[1], &account.profiles)
        .unwrap();
    assert!(home.rows.is_empty());
    assert_eq!(
        installed_addons(&cloud, generation, &shared).unwrap().len(),
        1
    );
    home_order(&cloud, generation, &shared).unwrap();
    assert_eq!(requests.lock().unwrap().len(), 5);
    let own = home
        .begin(&account.owner, &account.profiles[2], &account.profiles)
        .unwrap();
    assert_eq!(own.addon_profile, 3);
    assert!(!home.accepts(&shared));
}

#[test]
fn home_order_preserves_new_catalogs_and_empty_settings_clear_disabled_rows() {
    let addons = vec![addon(
        "fixture",
        json!([
        {"id":"first","type":"movie","name":"First"},
        {"id":"second","type":"movie","name":"Second"},
        {"id":"new","type":"series","name":"New"}]),
    )];
    let order = HomeOrder::parse(json!([{ "settings_json": { "items":[
        {"addon_id":"fixture","type":"movie","catalog_id":"first","enabled":false,"order":0},
        {"addon_id":"fixture","type":"movie","catalog_id":"second","order":1}]} }]))
    .unwrap();
    let rows = order.rows(&addons);
    assert_eq!(
        rows.iter().map(|row| row.name.as_str()).collect::<Vec<_>>(),
        ["Second", "New"]
    );
    assert!(Arc::ptr_eq(&rows[0].addon.manifest, &addons[0].manifest));
    let empty = HomeOrder::parse(json!([{ "settings_json":{"items":[]} }])).unwrap();
    assert_eq!(
        empty
            .rows(&addons)
            .iter()
            .map(|row| row.name.as_str())
            .collect::<Vec<_>>(),
        ["First", "Second", "New"]
    );
}

#[test]
fn addon_paths_preserve_private_query_and_encode_ids_and_search_extras() {
    let url = addon_endpoint(
        "https://addon.example.invalid/config/manifest.json?key=fixture%3Dkey",
        Some(("stream", "series", "tt123:1:2")),
        &[],
    )
    .unwrap();
    assert_eq!(url.path(), "/config/stream/series/tt123:1:2.json");
    assert_eq!(url.query_pairs().next().unwrap().1, "fixture=key");
    let search = addon_endpoint(
        "https://addon.example.invalid/",
        Some(("catalog", "movie", "safe/id")),
        &[("search".to_owned(), "space + / 日".to_owned())],
    )
    .unwrap();
    assert!(search.path().contains("safe%2Fid"));
    assert!(search
        .path()
        .ends_with("search=space+%2B+%2F+%E6%97%A5.json"));
}

#[test]
fn catalogs_details_and_episode_sources_keep_the_requested_identity() {
    let addon = addon("fixture", json!([{ "id":"home","type":"series" }]));
    let transport = fake(|request| {
        assert_eq!(
            request.header_value("accept-language"),
            Some("en-US,en;q=0.9")
        );
        let result = if request.url.path().contains("/catalog/") {
            json!({"metas":[{"id":"tt123","name":"Space","description":null,"genres":null,"imdbRating":8.5}, {"id":"","name":"Invalid"}]})
        } else if request.url.path().contains("/meta/") {
            json!({"meta":{"id":"tt123","name":"Space","videos":[
                {"id":"tt123:2:1","season":2,"episode":1,"title":"Later"},
                {"id":"tt123:1:2","season":1,"episode":2,"title":"First"}]}})
        } else {
            assert!(request
                .url
                .path()
                .ends_with("/stream/series/tt123:1:2.json"));
            json!({"streams":[{"name":"Fixture source","title":"1080p","description":"Direct source","url":"https://media.example.invalid/movie.mkv", "behaviorHints":{"proxyHeaders":{"request":{"Referer":"https://referer.example.invalid"}}}}]})
        };
        Ok(response(200, result))
    });
    let service = CatalogService::new(transport);
    let rows = HomeOrder::default().rows(std::slice::from_ref(&addon));
    let titles = service.row(&rows[0], 0).unwrap();
    assert_eq!(titles.len(), 1);
    assert_eq!(titles[0].kind, "series");
    assert_eq!(titles[0].imdb_rating, "8.5");
    let detail = service.detail(&addon, "series", "tt123").unwrap();
    assert_eq!(detail.sorted_episodes()[0].id, "tt123:1:2");
    let sources = service.sources(&addon, "series", "tt123:1:2").unwrap();
    assert_eq!(sources[0].source.title, "1080p");
    assert_eq!(sources[0].source.description, "Direct source");
    assert_eq!(
        sources[0].source.behavior.proxy_headers.request["Referer"],
        "https://referer.example.invalid"
    );
}

#[test]
fn resource_scopes_and_required_search_catalogs_are_respected() {
    let manifest = serde_json::from_value::<nuvio_rust_app::catalog::Manifest>(
        json!({"id":"fixture","name":"Fixture","types":["movie"],
        "resources":[{"name":"stream","types":["series"],"idPrefixes":["tt"]}],
        "catalogs":[{"id":"search","type":"movie","extra":[{"name":"search","isRequired":true}]}]}),
    )
    .unwrap();
    assert!(manifest.supports("stream", "series", "tt123:1:2"));
    assert!(!manifest.supports("stream", "movie", "tt123"));
    assert!(!manifest.supports("stream", "series", "tmdb:123"));
    assert!(manifest.catalogs[0].home_extras().is_none());
    let _: Source = serde_json::from_value(
        json!({"title":"Title","description":"Description","behaviorHints":null,"subtitles":null}),
    )
    .unwrap();
    let _: Title =
        serde_json::from_value(json!({"id":"tt123","name":"Title","imdbRating":null})).unwrap();
    let _: Profile = serde_json::from_value(
        json!({"profile_index":2,"uses_primary_plugins":true,"uses_primary_addons":false}),
    )
    .unwrap();
}
