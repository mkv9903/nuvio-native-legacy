use nuvio_backend::{
    addons::ADDON_RESPONSE_LIMIT,
    catalog_titles::{CatalogReply, CatalogTitle},
    error::Error,
};
use serde_json::{json, Value};

fn title(value: Value, kind: &str) -> CatalogTitle {
    CatalogTitle::from_meta(&value, kind).expect("usable title")
}

#[test]
fn root_artwork_wins_regardless_of_key_order_and_nested_art_is_a_fallback() {
    for body in [
        r#"{"id":"tt0903747","name":"Title","_providerArt":{"poster":"https://image.tmdb.org/t/p/w500/nested.jpg","background":"https://image.tmdb.org/t/p/original/nested.jpg","logo":"https://nested.invalid/logo.png"},"poster":"https://poster.invalid/custom.jpg","background":"https://root.invalid/bg.jpg","logo":"https://root.invalid/logo.png"}"#,
        r#"{"poster":"https://poster.invalid/custom.jpg","background":"https://root.invalid/bg.jpg","logo":"https://root.invalid/logo.png","id":"tt0903747","name":"Title","_providerArt":{"poster":"https://nested.invalid/poster.jpg"}}"#,
    ] {
        let parsed = title(serde_json::from_str(body).unwrap(), "series");
        assert_eq!(parsed.poster, "https://poster.invalid/custom.jpg");
        assert_eq!(parsed.background, "https://root.invalid/bg.jpg");
        assert_eq!(parsed.logo, "https://root.invalid/logo.png");
    }
    let parsed = title(
        json!({"id":"tt1","name":"Title","poster":null,"_providerArt":{"poster":"https://nested.invalid/poster.jpg"}}),
        "movie",
    );
    assert_eq!(parsed.poster, "https://nested.invalid/poster.jpg");
    assert_eq!(parsed.background, parsed.poster);
    let meta = json!({"meta":{"id":"tt1","name":"Title","poster":"https://root.invalid/poster.jpg","_providerArt":{"poster":"https://nested.invalid/poster.jpg"}}});
    assert_eq!(
        title(meta["meta"].clone(), "movie").poster,
        "https://root.invalid/poster.jpg"
    );
}

#[test]
fn root_movie_and_series_types_override_catalog_types_but_trailer_types_do_not() {
    let parsed = title(
        json!({"trailers":[{"type":"Trailer"}],"id":"tt13293588","type":"series","name":"Mushoku Tensei","poster":"https://p.invalid/x.jpg"}),
        "anime",
    );
    assert_eq!(parsed.media_type, "series");
    for (own, catalog, expected) in [
        (Value::Null, "anime", "anime"),
        (json!("other"), "movie", "movie"),
        (json!("movie"), "anime", "movie"),
    ] {
        let parsed = title(
            json!({"id":"id","type":own,"name":"Title","poster":"https://p.invalid/x.jpg"}),
            catalog,
        );
        assert_eq!(parsed.media_type, expected);
    }
}

#[test]
fn provider_identity_and_imdb_hints_are_separate_and_complete_strings_survive_round_trip() {
    let id = format!("channel:{}:日", "configuration-".repeat(600));
    let poster = format!(
        "https://poster.invalid/render?private={}&language=en",
        "configured-art-".repeat(700)
    );
    let description = "Full description. ".repeat(1000);
    let parsed = title(
        json!({"id":id,"imdb_id":"tt1234567","name":"Long title 日","poster":poster,"description":description}),
        "channel",
    );
    assert_eq!(parsed.id, id);
    assert_eq!(parsed.imdb_id.as_deref(), Some("tt1234567"));
    assert_eq!(parsed.poster, poster);
    assert_eq!(parsed.description, description);
    let restored: CatalogTitle =
        serde_json::from_slice(&serde_json::to_vec(&parsed).unwrap()).unwrap();
    assert_eq!(restored, parsed);
    let legacy = title(
        json!({"imdb_id":"tt1","name":"Title","poster":"https://p.invalid/x.jpg"}),
        "movie",
    );
    assert_eq!(legacy.id, "tt1");
    assert!(!format!("{parsed:?}").contains("private="));
}

#[test]
fn empty_catalog_is_authoritative_mixed_entries_skip_and_wholly_unusable_lists_fail() {
    assert!(CatalogReply::decode(br#"{"metas":[]}"#, "movie")
        .unwrap()
        .titles
        .is_empty());
    let parsed = CatalogReply::decode(br#"{"metas":[null,{}, {"id":"bad","name":"No poster"},{"id":"tt1","name":"Valid","poster":"https://p.invalid/x.jpg"}]}"#, "movie").unwrap();
    assert_eq!(parsed.titles.len(), 1);
    assert_eq!(parsed.skipped, 3);
    for invalid in [
        r#"{"metas":[{}]}"#,
        r#"{"metas":[{"name":"No ID","poster":"https://p.invalid/x.jpg"}]}"#,
        r#"{"metas":null}"#,
        r#"{"metas":{},"other":[]}"#,
        r#"{"nested":{"metas":[]}}"#,
        r#"{"metas":[],"error":{"message":"provider failed"}}"#,
        r#"{"metas":[],"error":"failed"}"#,
        "{",
    ] {
        assert_eq!(
            CatalogReply::decode(invalid.as_bytes(), "movie"),
            Err(Error::InvalidData)
        );
    }
    assert!(CatalogTitle::from_meta(
        &json!({"id":"bad\0id","name":"Title","poster":"https://p.invalid/x.jpg"}),
        "movie"
    )
    .is_none());
    assert!(CatalogTitle::from_meta(
        &json!({"id":"id","name":"Title","poster":"bad\0url"}),
        "movie"
    )
    .is_none());
    assert!(CatalogTitle::from_meta(
        &json!({"id":"id","name":" ","poster":"https://p.invalid/x.jpg"}),
        "movie"
    )
    .is_none());
}

#[test]
fn safe_backdrops_and_logo_fallback_preserve_full_provider_urls() {
    let parsed = title(
        json!({"id":"id","name":"Title","poster":"https://p.invalid/x.jpg","background":"https://image.tmdb.org/t/p/original/scene.jpg?key=keep","logo":"https://p.invalid/x.jpg"}),
        "movie",
    );
    assert_eq!(
        parsed.background,
        "https://image.tmdb.org/t/p/w1280/scene.jpg?key=keep"
    );
    assert!(parsed.logo.is_empty());
    let svg = title(
        json!({"id":"id","name":"Title","poster":"https://p.invalid/x.jpg","logo":"https://p.invalid/LOGO.SVG?key=keep"}),
        "movie",
    );
    assert!(svg.logo.is_empty());
    let provider = title(
        json!({"id":"id","name":"Title","poster":"https://p.invalid/x.jpg","background":"https://provider.invalid/t/p/original/configured.jpg?key=keep"}),
        "movie",
    );
    assert_eq!(
        provider.background,
        "https://provider.invalid/t/p/original/configured.jpg?key=keep"
    );
}

#[test]
fn scene_artwork_keeps_the_catalog_and_recognized_backdrop_variants() {
    use nuvio_backend::catalog_scene::SceneTitle;
    for (background, tmdb, trakt) in [
        ("https://image.tmdb.org/t/p/w1280/scene.jpg", true, false),
        ("https://media.trakt.tv/images/backdrop.jpg", false, true),
        (
            "https://provider.invalid/image.tmdb.org/scene.jpg",
            false,
            false,
        ),
    ] {
        let parsed = SceneTitle::from(title(
            json!({"id":"id","name":"Title","poster":"https://p.invalid/poster.jpg","background":background}),
            "movie",
        ));
        assert_eq!(parsed.artwork.catalog_background, background);
        assert_eq!(!parsed.artwork.tmdb_background.is_empty(), tmdb);
        assert_eq!(!parsed.artwork.trakt_background.is_empty(), trakt);
    }
    let parsed = SceneTitle::from(title(
        json!({"id":"id","name":"Title","poster":"https://image.tmdb.org/t/p/w500/poster.jpg"}),
        "movie",
    ));
    assert!(parsed.artwork.tmdb_background.is_empty());
}

#[test]
fn english_display_metadata_preserves_full_domain_fields_and_optional_numeric_variants() {
    let parsed = title(
        json!({"id":"id","name":"Title","poster":"https://p.invalid/x.jpg","genres":["Animation","Drama","Fantasy"],"releaseInfo":"2011-2019","runtime":"148m","country":"Japan","imdbRating":"8.1","moviedb_id":"4242"}),
        "series",
    );
    assert_eq!(parsed.genre_text(), "TV Show  ·  Animation  ·  Drama");
    assert_eq!(parsed.metadata_text(), "2011  ·  148 min");
    assert_eq!(parsed.release_info, "2011-2019");
    assert_eq!(parsed.runtime, "148m");
    assert_eq!(parsed.genres.len(), 3);
    assert_eq!(parsed.rating, Some(8.1));
    assert_eq!(parsed.tmdb_id, Some(4242));
    for (rating, expected) in [
        (json!(10.0), Some(10.0)),
        (json!(85), Some(8.5)),
        (json!("NaN"), None),
        (json!(-1), None),
    ] {
        let parsed = title(
            json!({"id":"id","name":"Title","poster":"https://p.invalid/x.jpg","imdbRating":rating,"moviedb_id":4,"runtime":"2h 1min","releaseInfo":"2024–"}),
            "channel",
        );
        assert_eq!(parsed.rating, expected);
        assert!(parsed.tmdb_id.is_none());
        assert_eq!(parsed.metadata_text(), "2024  ·  2h 1min");
        assert_eq!(parsed.genre_text(), "Channel");
    }
}

#[test]
fn large_catalogs_have_no_item_or_text_buffer_limit_and_oversized_bodies_are_rejected() {
    let metas: Vec<_> = (0..800).map(|index| json!({"id":format!("provider:{index}"),"name":format!("Title {index}"),"poster":"https://p.invalid/x.jpg"})).collect();
    let parsed = CatalogReply::decode(
        &serde_json::to_vec(&json!({"metas":metas})).unwrap(),
        "movie",
    )
    .unwrap();
    assert_eq!(parsed.titles.len(), 800);
    assert_eq!(parsed.titles[799].id, "provider:799");
    assert_eq!(
        CatalogReply::decode(&vec![b' '; ADDON_RESPONSE_LIMIT + 1], "movie"),
        Err(Error::ResponseLimit)
    );
    assert_eq!(
        CatalogReply::decode(br#"{"metas":[]}"#, "bad\0type"),
        Err(Error::InvalidData)
    );
}

#[test]
fn renderer_quota_preserves_owned_values_and_still_validates_the_entire_json_document() {
    let bytes = serde_json::to_vec(
        &json!({"metas":[{"id":"first","name":"First","poster":"https://p.invalid/first.jpg"},
        {"id":"second","name":"Second","poster":"https://p.invalid/second.jpg"}]}),
    )
    .unwrap();
    let parsed = CatalogReply::decode_with_limit(&bytes, "movie", Some(1)).unwrap();
    drop(bytes);
    assert_eq!(parsed.titles.len(), 1);
    assert_eq!(parsed.titles[0].id, "first");
    assert_eq!(CatalogReply::decode_with_limit(br#"{"metas":[{"id":"first","name":"First","poster":"https://p.invalid/first.jpg"}, {broken]}"#,"movie",Some(1)),Err(Error::InvalidData));
    assert_eq!(
        CatalogReply::decode_with_limit(br#"{"metas":[]}"#, "movie", Some(0)),
        Err(Error::InvalidData)
    );
    let mixed = CatalogReply::decode(br#"{"metas":[{"id":"bad","name":"Bad","poster":"https://p.invalid/x.jpg","imdbRating":1e400},{"id":"good","name":"Good","poster":"https://p.invalid/x.jpg"}]}"#,"movie").unwrap();
    assert_eq!(mixed.titles[0].id, "good");
    assert_eq!(mixed.skipped, 1);
}
