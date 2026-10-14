mod support;
use nuvio_backend::{
    addons::AddonUrl,
    catalog_scene::*,
    catalog_titles::CatalogTitle,
    content_context::{AddonScope, ContentContext, ContentScope},
    error::Error,
    model::ProfileId,
    scene_cache::{SceneCache, SCENE_CACHE_LIMIT},
    TEST_APP_ID,
};
use serde_json::json;
use std::{
    fs,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc, Barrier,
    },
    thread,
};
use support::Directory;

fn scope(profile: u32) -> ContentScope {
    ContentScope {
        member: "member".into(),
        owner: "shared-owner".into(),
        profile: ProfileId::new(profile).unwrap(),
    }
}
fn scene(id: &str) -> CatalogScene {
    let metadata = CatalogTitle::from_meta(
        &json!({
            "id":id, "imdb_id":"tt1234567", "type":"series", "name":"Title 日",
            "poster":"https://art.invalid/poster", "background":"https://art.invalid/background",
            "genres":["Drama"], "imdbRating":10, "moviedb_id":987
        }),
        "series",
    )
    .unwrap();
    let mut title = SceneTitle::from(metadata);
    title.origin = "provider-id".into();
    CatalogScene {
        titles: vec![title],
        rows: vec![CatalogRow {
            key: "provider_series_catalog".into(),
            name: "Series row".into(),
            media_type: "series".into(),
            source: Some(CatalogSource {
                addon: AddonUrl::parse("https://provider.invalid/config?private=preserve").unwrap(),
                media_type: "series".into(),
                catalog_id: "catalog".into(),
                extras: vec![("genre".into(), "Sci-Fi 日".into())],
            }),
            titles: 0..1,
            state: CatalogRowState::Ready,
        }],
    }
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
                .starts_with("scene-cache-")
        })
        .unwrap()
}
fn no_temporaries(directory: &Directory) {
    assert!(fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .all(|entry| !entry
            .unwrap()
            .file_name()
            .to_str()
            .unwrap()
            .ends_with(".tmp")));
}

#[test]
fn complete_title_scene_round_trips_art_cast_resume_seasons_and_commercial_availability() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let mut expected = scene(&format!("kitsu:{}", "identity".repeat(900)));
    let title = &mut expected.titles[0];
    let art = format!("https://art.invalid/{}", "configured-path".repeat(900));
    title.metadata.name = "Complete title 日".repeat(300);
    title.metadata.poster = art.clone();
    title.metadata.description = "Complete synopsis 日".repeat(300);
    title.artwork = ArtworkVariants {
        catalog_background: art.clone(),
        tmdb_background: "tmdb".into(),
        trakt_background: "trakt".into(),
        logo_language: "en".into(),
        logo_language_url: "selected-logo".into(),
    };
    title.display = TitleDisplay {
        genre_text: Some("TV Show  ·  Drama".into()),
        metadata_text: Some("2026  ·  48 min".into()),
        classification: "TV-14".into(),
        director: "Director".into(),
    };
    title.cast = (0..25)
        .map(|number| CastMember {
            name: format!("Actor {number} {}", "full-name".repeat(40)),
            role: "Character".into(),
            photo: art.clone(),
            tmdb_id: Some(5_000_000_000 + number),
        })
        .collect();
    title.availability = Availability {
        subscription: Some(CommercialProvider {
            name: "Stream".into(),
            logo: art.clone(),
        }),
        rental: Some(CommercialProvider {
            name: "Rent".into(),
            logo: art.clone(),
        }),
        purchase: Some(CommercialProvider {
            name: "Buy".into(),
            logo: art.clone(),
        }),
    };
    title.resume = ResumeInfo {
        percentage: 47,
        season: 5,
        episode: 13,
        remaining_minutes: 24,
        episode_name: "Episode".into(),
        last_watched_ms: 1_234_567_890_123,
    };
    title.seasons = (0..80).collect();
    title.on_watchlist = true;
    title.in_collection = true;
    expected.rows[0].key = "complete-row-key".repeat(300);
    expected.rows[0].source.as_mut().unwrap().addon = AddonUrl::parse(&format!(
        "https://provider.invalid/{}?private=keep",
        "config".repeat(1200)
    ))
    .unwrap();
    cache.save(&scope(2), &expected, &|| true).unwrap();
    let actual = SceneCache::open(&directory.path)
        .unwrap()
        .load(&scope(2), &|| true)
        .unwrap()
        .unwrap();
    assert_eq!(actual, expected);
    assert_eq!(actual.titles[0].metadata.rating, Some(10.0));
    assert_ne!(
        actual.titles[0].metadata.id,
        actual.titles[0].metadata.imdb_id.as_deref().unwrap()
    );
    let path = file(&directory);
    let name = path.file_name().unwrap().to_str().unwrap();
    assert_eq!(name.len(), "scene-cache-".len() + 64 + ".json".len());
    assert!(!name.contains("member") && !name.contains("private"));
    let bytes = fs::read(&path).unwrap();
    let envelope: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
    assert_eq!(envelope["version"], 1);
    assert_eq!(envelope["language"], "en");
    assert!(!String::from_utf8(bytes).unwrap().contains("rust_title"));
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(path).unwrap().permissions().mode() & 0o777,
            0o600
        );
    }
    no_temporaries(&directory);
}

#[test]
fn member_owner_and_selected_profile_are_independent_of_primary_addon_inheritance() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let selected = scope(2);
    cache.save(&selected, &scene("target"), &|| true).unwrap();
    let mut variants = vec![scope(1), scope(3)];
    let mut other = selected.clone();
    other.member = "other-member".into();
    variants.push(other);
    let mut other = selected.clone();
    other.owner = "other-owner".into();
    variants.push(other);
    for other in variants {
        assert!(cache.load(&other, &|| true).unwrap().is_none());
    }
    let context = ContentContext {
        account_generation: 1,
        content_generation: 2,
        profile: ProfileId::new(2).unwrap(),
        addons: AddonScope {
            member: "member".into(),
            owner: "shared-owner".into(),
            profile: ProfileId::new(1).unwrap(),
        },
        force_primary: false,
    };
    assert_eq!(ContentScope::from(&context), selected);
    assert_eq!(
        cache.load(&selected, &|| true).unwrap().unwrap().titles[0]
            .metadata
            .id,
        "target"
    );
    let mut unconfirmed = selected.clone();
    unconfirmed.owner.clear();
    assert_eq!(
        cache.save(&unconfirmed, &scene("wrong"), &|| true),
        Err(Error::InvalidData)
    );
}

#[test]
fn confirmed_empty_scenes_and_rows_remove_old_titles_without_losing_row_identity() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("old"), &|| true).unwrap();
    let mut empty_row = scene("unused");
    empty_row.titles.clear();
    empty_row.rows[0].titles = 0..0;
    empty_row.rows[0].state = CatalogRowState::ConfirmedEmpty;
    cache.save(&scope(1), &empty_row, &|| true).unwrap();
    assert_eq!(cache.load(&scope(1), &|| true).unwrap().unwrap(), empty_row);
    cache
        .save(&scope(1), &CatalogScene::default(), &|| true)
        .unwrap();
    assert_eq!(
        cache.load(&scope(1), &|| true).unwrap().unwrap(),
        CatalogScene::default()
    );
}

#[test]
fn invalid_records_windows_and_request_values_cannot_replace_a_valid_scene() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let original = scene("original");
    cache.save(&scope(1), &original, &|| true).unwrap();
    let mut invalid = Vec::new();
    let mut changed = original.clone();
    changed.titles[0].metadata.id.clear();
    invalid.push(changed);
    let mut changed = original.clone();
    changed.titles[0].metadata.poster.push('\0');
    invalid.push(changed);
    let mut changed = original.clone();
    changed.titles[0].metadata.rating = Some(10.01);
    invalid.push(changed);
    let mut changed = original.clone();
    changed.titles[0].resume.percentage = 101;
    invalid.push(changed);
    let mut changed = original.clone();
    changed.rows[0].titles = 0..2;
    invalid.push(changed);
    let mut changed = original.clone();
    changed.rows[0].titles = 2..1;
    invalid.push(changed);
    let mut changed = original.clone();
    changed.rows[0].state = CatalogRowState::ConfirmedEmpty;
    invalid.push(changed);
    let mut changed = original.clone();
    changed.rows[0].source.as_mut().unwrap().catalog_id.clear();
    invalid.push(changed);
    let mut changed = original.clone();
    changed.rows[0].source.as_mut().unwrap().extras[0]
        .1
        .push('\0');
    invalid.push(changed);
    for changed in invalid {
        assert_eq!(
            cache.save(&scope(1), &changed, &|| true),
            Err(Error::InvalidData)
        );
        assert_eq!(cache.load(&scope(1), &|| true).unwrap().unwrap(), original);
    }
}

#[test]
fn unreadable_or_foreign_envelopes_are_retained_and_a_fresh_valid_write_can_recover() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("original"), &|| true).unwrap();
    let path = file(&directory);
    let original: serde_json::Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
    for (field, value) in [
        ("version", json!(99)),
        ("app_id", json!("production")),
        ("language", json!("pt")),
        (
            "scope",
            json!({"member":"member","owner":"other-owner","profile":1}),
        ),
        (
            "scope",
            json!({"member":"member","owner":"shared-owner","profile":0}),
        ),
        ("scene", json!({})),
    ] {
        let mut wrong = original.clone();
        wrong[field] = value;
        let bytes = serde_json::to_vec(&wrong).unwrap();
        fs::write(&path, &bytes).unwrap();
        assert_eq!(cache.load(&scope(1), &|| true), Err(Error::InvalidData));
        assert_eq!(fs::read(&path).unwrap(), bytes);
    }
    for bytes in [b"{".as_slice(), b"null", b"\xff"] {
        fs::write(&path, bytes).unwrap();
        assert_eq!(cache.load(&scope(1), &|| true), Err(Error::InvalidData));
        assert_eq!(fs::read(&path).unwrap(), bytes);
    }
    cache
        .save(&scope(1), &scene("recovered"), &|| true)
        .unwrap();
    assert_eq!(
        cache.load(&scope(1), &|| true).unwrap().unwrap().titles[0]
            .metadata
            .id,
        "recovered"
    );
}

#[test]
fn cancellation_after_temporary_write_preserves_the_previous_scene_and_cleans_the_temporary() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let original = scene("original");
    cache.save(&scope(1), &original, &|| true).unwrap();
    let calls = AtomicUsize::new(0);
    // Initial capture and lock checks pass; the final commit check cancels.
    assert_eq!(
        cache.save(&scope(1), &scene("late"), &|| calls
            .fetch_add(1, Ordering::SeqCst)
            < 2),
        Err(Error::Cancelled)
    );
    assert_eq!(calls.load(Ordering::SeqCst), 3);
    assert_eq!(cache.load(&scope(1), &|| true).unwrap().unwrap(), original);
    assert_eq!(
        cache.save(&scope(1), &scene("late"), &|| false),
        Err(Error::Cancelled)
    );
    no_temporaries(&directory);
}

#[test]
fn cancelled_cache_reads_cannot_return_an_old_context_scene() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("original"), &|| true).unwrap();
    for accepted in [0, 1, 2] {
        let calls = AtomicUsize::new(0);
        assert_eq!(
            cache.load(&scope(1), &|| calls.fetch_add(1, Ordering::SeqCst)
                < accepted),
            Err(Error::Cancelled)
        );
    }
    assert_eq!(
        cache.load(&scope(1), &|| true).unwrap().unwrap().titles[0]
            .metadata
            .id,
        "original"
    );
}

#[test]
fn damaged_cache_from_a_cancelled_context_does_not_report_an_active_profile_error() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("original"), &|| true).unwrap();
    let path = file(&directory);
    fs::write(&path, "{").unwrap();
    let calls = AtomicUsize::new(0);
    assert_eq!(
        cache.load(&scope(1), &|| calls.fetch_add(1, Ordering::SeqCst) < 2),
        Err(Error::Cancelled)
    );
    assert_eq!(fs::read_to_string(path).unwrap(), "{");
    assert_eq!(cache.load(&scope(1), &|| true), Err(Error::InvalidData));
}

#[test]
fn domain_arrays_have_no_old_native_title_row_cast_or_season_limits() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let mut expected = scene("first");
    expected.titles = (0..2101)
        .map(|index| scene(&format!("title-{index}")).titles.remove(0))
        .collect();
    expected.rows = (0..81)
        .map(|index| CatalogRow {
            key: format!("row-{index}"),
            name: "Row".into(),
            media_type: "series".into(),
            source: None,
            titles: index..index + 1,
            state: CatalogRowState::Ready,
        })
        .collect();
    cache.save(&scope(1), &expected, &|| true).unwrap();
    assert_eq!(cache.load(&scope(1), &|| true).unwrap().unwrap(), expected);
}

#[test]
fn aggregate_cache_byte_limits_never_truncate_or_replace_valid_data() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let original = scene("original");
    cache.save(&scope(1), &original, &|| true).unwrap();
    let mut oversized = scene("large");
    oversized.titles[0].metadata.description = "x".repeat(SCENE_CACHE_LIMIT as usize);
    assert_eq!(
        cache.save(&scope(1), &oversized, &|| true),
        Err(Error::ResponseLimit)
    );
    assert_eq!(cache.load(&scope(1), &|| true).unwrap().unwrap(), original);
    drop(oversized);
    fs::OpenOptions::new()
        .write(true)
        .open(file(&directory))
        .unwrap()
        .set_len(SCENE_CACHE_LIMIT + 1)
        .unwrap();
    assert_eq!(cache.load(&scope(1), &|| true), Err(Error::ResponseLimit));
    no_temporaries(&directory);
}

#[test]
fn independent_handles_publish_whole_scenes_during_concurrent_reads() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("original"), &|| true).unwrap();
    let barrier = Arc::new(Barrier::new(3));
    thread::scope(|threads| {
        for writer in 0..2 {
            let handle = SceneCache::open(&directory.path).unwrap();
            let barrier = barrier.clone();
            threads.spawn(move || {
                barrier.wait();
                for iteration in 0..30 {
                    let id = format!("writer-{writer}-{iteration}");
                    let mut next = scene(&id);
                    next.titles[0].metadata.name = id;
                    handle.save(&scope(1), &next, &|| true).unwrap();
                }
            });
        }
        barrier.wait();
        for _ in 0..100 {
            let loaded = cache.load(&scope(1), &|| true).unwrap().unwrap();
            let title = &loaded.titles[0].metadata;
            if title.id != "original" {
                assert_eq!(title.id, title.name);
            }
            assert_eq!(loaded.rows[0].titles, 0..1);
        }
    });
    no_temporaries(&directory);
}

#[test]
fn legacy_files_are_not_imported_and_clear_only_removes_scene_files() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    fs::write(directory.path.join("catalogo-rede.bin"), "legacy").unwrap();
    assert!(cache.load(&scope(1), &|| true).unwrap().is_none());
    let unrelated = directory
        .path
        .join("rust-backend-v1/addon-pending-keep.json");
    fs::write(&unrelated, "saved edit").unwrap();
    cache.save(&scope(1), &scene("one"), &|| true).unwrap();
    cache.save(&scope(2), &scene("two"), &|| true).unwrap();
    cache.clear().unwrap();
    assert!(cache.load(&scope(1), &|| true).unwrap().is_none());
    assert!(cache.load(&scope(2), &|| true).unwrap().is_none());
    assert_eq!(fs::read_to_string(unrelated).unwrap(), "saved edit");
    assert_eq!(
        fs::read_to_string(directory.path.join("catalogo-rede.bin")).unwrap(),
        "legacy"
    );
}

#[test]
fn private_directory_guards_and_redacted_debug_keep_configuration_out_of_diagnostics() {
    let directory = Directory::new();
    assert!(matches!(
        SceneCache::open(&directory.parent),
        Err(Error::ProductionData)
    ));
    let data = scene("private-identity");
    let diagnostics = format!("{data:?} {:?}", data.titles[0]);
    assert!(
        !diagnostics.contains("private-identity")
            && !diagnostics.contains("https://")
            && !diagnostics.contains("preserve")
    );
    assert_eq!(directory.path.file_name().unwrap(), TEST_APP_ID);
}

#[cfg(unix)]
#[test]
fn cache_symlinks_are_not_read_and_atomic_recovery_cannot_modify_their_target() {
    use std::os::unix::fs::symlink;
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(1), &scene("original"), &|| true).unwrap();
    let path = file(&directory);
    let outside = directory.parent.join("unrelated-private-data");
    fs::write(&outside, "untouched").unwrap();
    fs::remove_file(&path).unwrap();
    symlink(&outside, &path).unwrap();
    assert_eq!(cache.load(&scope(1), &|| true), Err(Error::Storage));
    cache
        .save(&scope(1), &scene("recovered"), &|| true)
        .unwrap();
    assert_eq!(fs::read_to_string(outside).unwrap(), "untouched");
    assert_eq!(
        cache.load(&scope(1), &|| true).unwrap().unwrap().titles[0]
            .metadata
            .id,
        "recovered"
    );
}
