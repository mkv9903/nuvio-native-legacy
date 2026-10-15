#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    catalog_scene::{CatalogRowState, CommercialProvider},
    content_context::ContentScope,
    model::ProfileId,
    scene_cache::SceneCache,
    scene_snapshot::{nuvio_row_release, nuvio_scene_row, SceneSnapshot},
    title_snapshot::TitleSnapshot,
};
use std::{
    ffi::{c_char, c_void},
    sync::Arc,
};
use support::{scene::complete_scene, Directory};

#[link(name = "nuvio_c_ui")]
extern "C" {
    fn native_title_create(snapshot: *const c_void) -> *mut c_void;
    fn native_title_destroy(item: *mut c_void);
    fn native_title_overlay_poster(item: *mut c_void, poster: *const c_char);
    fn native_title_overlay_resume(
        item: *mut c_void,
        percentage: i32,
        season: i32,
        episode: i32,
        remaining: i32,
        watched: i64,
    );
    fn native_title_overlay_cast(item: *mut c_void, mode: i32);
    fn native_title_capture(item: *const c_void, row: *const c_void) -> *const c_void;
    fn native_title_capture_live_c_values() -> *const c_void;
}
fn scope() -> ContentScope {
    ContentScope {
        member: "member".into(),
        owner: "owner".into(),
        profile: ProfileId::new(2).unwrap(),
    }
}

#[test]
fn actual_c_projection_round_trip_preserves_full_values_extras_ratings_and_array_tails() {
    let mut expected = complete_scene();
    expected.titles[0].metadata.rating = Some(8.45);
    let source = Arc::new(SceneSnapshot::new(expected.clone()).unwrap());
    let weak = Arc::downgrade(&source.titles[0]);
    unsafe {
        let item = native_title_create(Arc::into_raw(source.titles[0].clone()).cast());
        let row = nuvio_scene_row(Arc::as_ptr(&source), 0);
        drop(source);
        let captured = native_title_capture(item, row.cast()).cast::<SceneSnapshot>();
        native_title_destroy(item);
        nuvio_row_release(row);
        assert!(
            weak.upgrade().is_none(),
            "capture retained a borrowed C record"
        );
        assert!(!captured.is_null());
        let captured = Arc::from_raw(captured);
        assert_eq!(captured.to_scene(), expected);
        let directory = Directory::new();
        let cache = SceneCache::open(&directory.path).unwrap();
        cache
            .save(&scope(), &captured.to_scene(), &|| true)
            .unwrap();
        drop(captured);
        assert_eq!(
            SceneSnapshot::restore(&cache, &scope(), &|| true)
                .unwrap()
                .unwrap()
                .to_scene(),
            expected
        );
    }
}

#[test]
fn live_c_enrichment_and_progress_changes_update_the_rust_scene_without_clipping_other_data() {
    let mut expected = complete_scene();
    let title = Arc::new(TitleSnapshot::from_scene(expected.titles[0].clone()));
    unsafe {
        let item = native_title_create(Arc::into_raw(title).cast());
        native_title_overlay_poster(item, c"https://art.invalid/updated".as_ptr());
        native_title_overlay_resume(item, 63, 8, 14, 7, 2345678901234);
        native_title_overlay_cast(item, 1);
        let captured = native_title_capture(item, std::ptr::null()).cast::<SceneSnapshot>();
        native_title_destroy(item);
        assert!(!captured.is_null());
        let actual = Arc::from_raw(captured).to_scene();
        expected.rows.clear();
        let title = &mut expected.titles[0];
        title.metadata.poster = "https://art.invalid/updated".into();
        title.resume.percentage = 63;
        title.resume.season = 8;
        title.resume.episode = 14;
        title.resume.remaining_minutes = 7;
        title.resume.last_watched_ms = 2345678901234;
        title.cast[0].role = "Updated role".into();
        title.seasons[0] = 9;
        assert_eq!(actual, expected);
    }
}

#[test]
fn explicit_c_array_removal_or_shorter_counts_clear_the_previous_rust_enrichment() {
    for (mode, cast, seasons) in [(0, 0, 0), (4, 2, 3)] {
        let expected = complete_scene();
        unsafe {
            let item = native_title_create(
                Arc::into_raw(Arc::new(TitleSnapshot::from_scene(
                    expected.titles[0].clone(),
                )))
                .cast(),
            );
            native_title_overlay_cast(item, mode);
            let captured = native_title_capture(item, std::ptr::null()).cast::<SceneSnapshot>();
            native_title_destroy(item);
            assert!(!captured.is_null());
            let captured = Arc::from_raw(captured);
            assert!(captured.titles[0].record.cast == expected.titles[0].cast[..cast]);
            assert_eq!(
                captured.titles[0].record.seasons,
                expected.titles[0].seasons[..seasons]
            );
        }
    }
}

#[test]
fn invalid_live_c_records_reject_the_capture_and_leave_a_valid_rust_cache_untouched() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    let expected = complete_scene();
    cache.save(&scope(), &expected, &|| true).unwrap();
    unsafe {
        for mode in [2, 3] {
            let item = native_title_create(
                Arc::into_raw(Arc::new(TitleSnapshot::from_scene(
                    expected.titles[0].clone(),
                )))
                .cast(),
            );
            native_title_overlay_cast(item, mode);
            assert!(native_title_capture(item, std::ptr::null()).is_null());
            native_title_destroy(item);
        }
        let item = native_title_create(
            Arc::into_raw(Arc::new(TitleSnapshot::from_scene(
                expected.titles[0].clone(),
            )))
            .cast(),
        );
        native_title_overlay_resume(item, 101, 5, 13, 24, 1234);
        assert!(native_title_capture(item, std::ptr::null()).is_null());
        native_title_destroy(item);
    }
    assert_eq!(cache.load(&scope(), &|| true).unwrap().unwrap(), expected);
}

#[test]
fn live_c_records_without_a_rust_source_become_owned_rust_models_with_independent_identity() {
    unsafe {
        let captured = native_title_capture_live_c_values().cast::<SceneSnapshot>();
        assert!(!captured.is_null());
        let actual = Arc::from_raw(captured).to_scene();
        let title = &actual.titles[0];
        assert_eq!(title.metadata.id, "kitsu:41370");
        assert_eq!(title.metadata.imdb_id, None);
        assert_eq!(title.metadata.rating, Some(8.5));
        assert_eq!(title.metadata.tmdb_id, Some(987));
        assert_eq!(title.metadata.name, "Live title");
        assert_eq!(title.metadata.description, "Live description");
        assert_eq!(title.display.genre_text.as_deref(), Some("TV Show · Drama"));
        assert_eq!(
            title.display.metadata_text.as_deref(),
            Some("2026 · 48 min")
        );
        assert_eq!(
            (
                title.resume.percentage,
                title.resume.season,
                title.resume.episode,
                title.resume.remaining_minutes
            ),
            (37, 2, 8, 19)
        );
        assert_eq!(title.resume.last_watched_ms, 123456789);
        assert!(title.on_watchlist);
        assert_eq!(title.seasons, [1, 2]);
        assert_eq!(title.cast[0].name, "Live actor");
        assert_eq!(
            title.availability.subscription.as_ref().unwrap().name,
            "Live subscription"
        );
        assert_eq!(actual.rows[0].state, CatalogRowState::Ready);
        assert_eq!(
            actual.rows[0].source.as_ref().unwrap().addon.as_str(),
            "https://provider.invalid/config?private=keep"
        );
    }
}

#[test]
fn display_nul_empty_provider_and_unrepresentable_c_scalars_survive_an_unchanged_projection() {
    let mut expected = complete_scene();
    let title = &mut expected.titles[0];
    title.metadata.name = "Name\0with NUL".into();
    title.metadata.description = "Description\0with NUL".into();
    title.display.genre_text = Some("Genre\0with NUL".into());
    title.resume.episode_name = "Episode\0with NUL".into();
    title.cast[0].name = "Actor\0name".into();
    title.resume.season = u32::MAX;
    title.seasons[0] = u32::MAX;
    title.availability.rental = Some(CommercialProvider {
        name: String::new(),
        logo: String::new(),
    });
    unsafe {
        let item = native_title_create(
            Arc::into_raw(Arc::new(TitleSnapshot::from_scene(
                expected.titles[0].clone(),
            )))
            .cast(),
        );
        let captured = native_title_capture(item, std::ptr::null()).cast::<SceneSnapshot>();
        native_title_destroy(item);
        assert!(!captured.is_null());
        expected.rows.clear();
        assert_eq!(Arc::from_raw(captured).to_scene(), expected);
    }
}
