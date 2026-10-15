#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    catalog_titles::CatalogTitle,
    scene_snapshot::SceneSnapshot,
    title_snapshot::{TitleField, TitleSnapshot},
};
use serde_json::json;
use std::{
    ffi::{c_char, c_void, CStr, CString},
    sync::Arc,
};
#[link(name = "nuvio_c_ui")]
extern "C" {
    fn native_title_create(snapshot: *const c_void) -> *mut c_void;
    fn native_title_clone(item: *const c_void) -> *mut c_void;
    fn native_title_copy_catalog(index: i32) -> *mut c_void;
    fn native_title_destroy(item: *mut c_void);
    fn native_title_text(item: *const c_void, field: u32) -> *const c_char;
    fn native_title_overlay_poster(item: *mut c_void, poster: *const c_char);
    fn native_title_projection_value(item: *const c_void, field: u32) -> i64;
    fn native_title_cast_projection(item: *const c_void, index: usize, field: u32)
        -> *const c_char;
    fn native_title_season_projection(item: *const c_void, index: usize) -> i32;
    fn native_title_publish(item: *const c_void);
    fn native_title_update(item: *const c_void);
    fn native_title_current() -> *const c_void;
    fn native_title_frame();
    fn native_title_find(id: *const c_char) -> i32;
    fn native_title_details_hold(item: *const c_void);
    fn native_title_details_revalidate() -> i32;
    fn native_title_details_selection() -> *const c_void;
    fn native_title_details_release();
    fn native_title_player_hold(item: *const c_void);
    fn native_title_player_selection() -> *const c_void;
    fn native_title_player_release();
    fn native_title_open_saved_menu(item: *const c_void);
    fn native_title_menu_selection() -> *const c_void;
    fn native_title_menu_release();
    fn native_title_set_channel(item: *const c_void);
    fn native_title_channel_id() -> *const c_char;
    fn native_title_queue_undo(item: *const c_void, intent: i32, clone_failure: i32);
    fn native_title_undo_id() -> *const c_char;
    fn native_title_undo_intent() -> i32;
    fn native_title_undo_executions() -> i32;
    fn native_title_reset_undo();
    fn action_notice_has_undo() -> i32;
    fn action_notice_button(button: i32);
    fn discovery_test_rating_paths(body: *const c_char, ratings: *mut i32) -> i32;
    fn discovery_test_round_create(item: *const c_void) -> *mut c_void;
    fn discovery_test_round_publish(handle: *mut c_void);
    fn discovery_test_round_abandon(handle: *mut c_void);
    fn discovery_test_round_complete(handle: *mut c_void);
    fn discovery_test_partial_lists(
        title: *const c_void,
        saved: *const c_void,
        repeats: i32,
    ) -> i32;
    fn native_title_compact_enrichment(rejected: *const c_void, kept: *const c_void)
        -> *mut c_void;
    fn native_title_filter_progress(finished: *const c_void, kept: *const c_void) -> *mut c_void;
}

#[test]
fn asynchronous_catalog_copy_retains_complete_values_after_home_is_replaced() {
    let id = format!("provider:{}", "full-value".repeat(200));
    let title = snapshot(&id, "poster");
    let weak = Arc::downgrade(&title);
    unsafe {
        let original = native_title_create(Arc::into_raw(title).cast());
        native_title_publish(original);
        native_title_destroy(original);
        let address = std::thread::spawn(|| native_title_copy_catalog(0) as usize)
            .join()
            .unwrap();
        let held = address as *mut c_void;
        assert!(!held.is_null());
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
        assert_eq!(get(held, TitleField::Id), id);
        assert!(weak.upgrade().is_some());
        native_title_destroy(held);
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn asynchronous_catalog_indices_never_wrap_to_another_title() {
    unsafe {
        assert!(native_title_copy_catalog(0).is_null());
        let title = snapshot("provider:only", "poster");
        let original = native_title_create(Arc::into_raw(title).cast());
        native_title_publish(original);
        native_title_destroy(original);
        assert!(native_title_copy_catalog(-1).is_null());
        assert!(native_title_copy_catalog(1).is_null());
        assert!(native_title_copy_catalog(i32::MAX).is_null());
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
}

#[test]
fn restored_scene_projects_all_existing_c_fields_while_retaining_complete_rust_records() {
    let expected = support::scene::complete_scene();
    let scene = SceneSnapshot::new(expected.clone()).unwrap();
    let weak = Arc::downgrade(&scene.titles[0]);
    unsafe {
        let item = native_title_create(Arc::into_raw(scene.titles[0].clone()).cast());
        drop(scene);
        assert!(!item.is_null());
        for (field, expected) in [
            (TitleField::CatalogBackground, "https://art.invalid/catalog"),
            (TitleField::TmdbBackground, "https://art.invalid/tmdb"),
            (TitleField::TraktBackground, "https://art.invalid/trakt"),
            (TitleField::LogoLanguage, "en"),
            (TitleField::LogoLanguageUrl, "https://art.invalid/logo"),
            (TitleField::Classification, "TV-14"),
            (TitleField::SubscriptionName, "Subscription"),
            (TitleField::SubscriptionLogo, "subscription-logo"),
            (TitleField::RentalName, "Rental"),
            (TitleField::RentalLogo, "rental-logo"),
            (TitleField::PurchaseName, "Purchase"),
            (TitleField::PurchaseLogo, "purchase-logo"),
            (TitleField::MetadataText, "2026 · 80 seasons"),
        ] {
            assert_eq!(get(item, field), expected);
        }
        for (field, expected) in [47, 5, 13, 24, 1_234_567_890_123, 1, 1, 12, 64]
            .into_iter()
            .enumerate()
        {
            assert_eq!(native_title_projection_value(item, field as u32), expected);
        }
        for (index, source) in expected.titles[0].cast.iter().take(12).enumerate() {
            for (field, source) in [
                source.name.as_str(),
                source.role.as_str(),
                source.photo.as_str(),
            ]
            .into_iter()
            .enumerate()
            {
                let projected =
                    CStr::from_ptr(native_title_cast_projection(item, index, field as u32))
                        .to_str()
                        .unwrap();
                assert!(!projected.is_empty());
                assert!(source.starts_with(projected));
            }
        }
        for season in 0..64 {
            assert_eq!(native_title_season_projection(item, season), season as i32);
        }
        assert_eq!(weak.upgrade().unwrap().record, expected.titles[0]);
        native_title_destroy(item);
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn cancelled_catalog_round_releases_ready_and_late_results_after_its_last_participant() {
    for worker_first in [false, true] {
        let title = snapshot("provider:cancelled", "poster");
        let weak = Arc::downgrade(&title);
        unsafe {
            let source = native_title_create(Arc::into_raw(title).cast());
            let round = discovery_test_round_create(source);
            assert!(!round.is_null());
            native_title_destroy(source);
            if worker_first {
                discovery_test_round_complete(round);
                assert!(weak.upgrade().is_some());
                discovery_test_round_abandon(round);
            } else {
                discovery_test_round_abandon(round);
                assert!(weak.upgrade().is_some());
                // This result finishes after the caller cancelled the round.
                let address = round as usize;
                std::thread::spawn(move || discovery_test_round_complete(address as *mut c_void))
                    .join()
                    .unwrap();
            }
        }
        assert!(weak.upgrade().is_none(), "a closed round leaked a result");
    }
}

#[test]
fn published_catalog_survives_round_cancellation_and_a_late_worker() {
    let id = format!("provider:{}", "long-identity".repeat(300));
    let title = snapshot(&id, "poster");
    let weak = Arc::downgrade(&title);
    unsafe {
        let source = native_title_create(Arc::into_raw(title).cast());
        let round = discovery_test_round_create(source);
        assert!(!round.is_null());
        native_title_destroy(source);
        discovery_test_round_publish(round);
        discovery_test_round_abandon(round);
        discovery_test_round_complete(round);
        assert_eq!(get(native_title_current(), TitleField::Id), id);
        assert!(weak.upgrade().is_some());
        native_title_publish(std::ptr::null());
        // The C catalog intentionally retains its most recently retired block.
        // Retire an empty block to replace that final reader grace period.
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn repeated_partial_home_publications_release_temporary_saved_list_tails() {
    let catalog = snapshot("provider:catalog", "poster");
    let saved = snapshot("provider:saved", "poster");
    let catalog_weak = Arc::downgrade(&catalog);
    let saved_weak = Arc::downgrade(&saved);
    unsafe {
        let catalog_item = native_title_create(Arc::into_raw(catalog).cast());
        let saved_item = native_title_create(Arc::into_raw(saved).cast());
        assert_eq!(
            discovery_test_partial_lists(catalog_item, saved_item, 40),
            1
        );
        native_title_destroy(catalog_item);
        native_title_destroy(saved_item);
        assert_eq!(
            get(native_title_current(), TitleField::Id),
            "provider:catalog"
        );
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(
        catalog_weak.upgrade().is_none(),
        "a Home batch leaked its owner"
    );
    assert!(
        saved_weak.upgrade().is_none(),
        "a saved-list tail leaked its owner"
    );
}

#[test]
fn enrichment_and_progress_compaction_drop_rejected_owners_and_transfer_survivors() {
    for compact in [
        native_title_compact_enrichment,
        native_title_filter_progress,
    ] {
        let rejected = snapshot("provider:rejected", "poster");
        let kept = snapshot("provider:kept", "poster");
        let rejected_weak = Arc::downgrade(&rejected);
        let kept_weak = Arc::downgrade(&kept);
        unsafe {
            let rejected_item = native_title_create(Arc::into_raw(rejected).cast());
            let kept_item = native_title_create(Arc::into_raw(kept).cast());
            let result = compact(rejected_item, kept_item);
            assert!(!result.is_null());
            native_title_destroy(rejected_item);
            native_title_destroy(kept_item);
            assert!(rejected_weak.upgrade().is_none());
            assert_eq!(get(result, TitleField::Id), "provider:kept");
            native_title_destroy(result);
        }
        assert!(
            kept_weak.upgrade().is_none(),
            "compaction duplicated ownership"
        );
    }
}
fn snapshot(id: &str, poster: &str) -> Arc<TitleSnapshot> {
    let title = CatalogTitle::from_meta(
        &json!({"id":id,"imdb_id":"tt1234567","type":"movie",
        "name":"A complete title 日".repeat(300),"poster":poster}),
        "movie",
    )
    .unwrap();
    Arc::new(TitleSnapshot::new(title, "provider".into()))
}
unsafe fn get(item: *const c_void, field: TitleField) -> String {
    unsafe { CStr::from_ptr(native_title_text(item, field as u32)) }
        .to_str()
        .unwrap()
        .into()
}
#[test]
fn actual_c_copies_keep_complete_rust_values_alive_and_allow_legacy_art_enrichment() {
    let id = format!("kitsu:{}", "identity".repeat(600));
    let poster = format!("https://art.invalid/{}", "configuration".repeat(800));
    let title = snapshot(&id, &poster);
    let weak = Arc::downgrade(&title);
    unsafe {
        let source = native_title_create(Arc::into_raw(title).cast());
        assert!(!source.is_null());
        let copy = native_title_clone(source);
        native_title_destroy(source);
        assert_eq!(get(copy, TitleField::Id), id);
        assert_eq!(get(copy, TitleField::ImdbId), "tt1234567");
        assert_eq!(get(copy, TitleField::Poster), poster);
        assert_eq!(
            get(copy, TitleField::Name),
            "A complete title 日".repeat(300)
        );
        native_title_overlay_poster(copy, c"https://art.invalid/enriched".as_ptr());
        assert_eq!(
            get(copy, TitleField::Poster),
            "https://art.invalid/enriched"
        );
        assert_eq!(weak.upgrade().unwrap().record.metadata.poster, poster);
        native_title_destroy(copy);
    }
    assert!(
        weak.upgrade().is_none(),
        "a C title copy leaked its native owner"
    );
}
#[test]
fn catalog_publication_replacement_and_held_selection_have_independent_lifetimes() {
    let id = format!("provider:{}", "complete-id".repeat(400));
    let title = snapshot(&id, "https://art.invalid/original");
    let weak = Arc::downgrade(&title);
    unsafe {
        let source = native_title_create(Arc::into_raw(title).cast());
        native_title_publish(source);
        native_title_destroy(source);
        let held = native_title_clone(native_title_current());
        assert_eq!(
            native_title_find(CString::new(id.clone()).unwrap().as_ptr()),
            0
        );
        for index in 0..4 {
            let replacement = snapshot(
                &format!("different-{index}"),
                "https://art.invalid/replacement",
            );
            let next = native_title_create(Arc::into_raw(replacement).cast());
            native_title_publish(next);
            native_title_destroy(next);
            native_title_frame();
            assert_eq!(get(held, TitleField::Id), id);
        }
        native_title_destroy(held);
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(
        weak.upgrade().is_none(),
        "retired catalog blocks leaked title owners"
    );
}
#[test]
fn in_place_metadata_updates_retire_borrowed_native_text_until_the_next_frame() {
    let original = snapshot("tt1", "https://art.invalid/original");
    let weak = Arc::downgrade(&original);
    unsafe {
        let source = native_title_create(Arc::into_raw(original).cast());
        native_title_publish(source);
        native_title_destroy(source);
        let borrowed = native_title_text(native_title_current(), TitleField::Poster as u32);
        let replacement = native_title_create(
            Arc::into_raw(snapshot("tt1", "https://art.invalid/replaced")).cast(),
        );
        native_title_update(replacement);
        native_title_destroy(replacement);
        assert_eq!(
            CStr::from_ptr(borrowed).to_str().unwrap(),
            "https://art.invalid/original"
        );
        native_title_frame();
        assert!(weak.upgrade().is_some());
        native_title_frame();
        assert!(weak.upgrade().is_none());
        assert_eq!(
            get(native_title_current(), TitleField::Poster),
            "https://art.invalid/replaced"
        );
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
}

#[test]
fn details_and_playback_selections_retain_full_identity_when_catalogs_change() {
    let id = format!("provider:{}", "same-prefix".repeat(500));
    let title = snapshot(&id, "poster");
    let weak = Arc::downgrade(&title);
    unsafe {
        let source = native_title_create(Arc::into_raw(title).cast());
        native_title_details_hold(source);
        native_title_player_hold(source);
        native_title_destroy(source);
        let other = native_title_create(Arc::into_raw(snapshot("other", "poster")).cast());
        native_title_publish(other);
        native_title_destroy(other);
        native_title_frame();
        assert_eq!(native_title_details_revalidate(), 1);
        assert_eq!(get(native_title_details_selection(), TitleField::Id), id);
        assert_eq!(get(native_title_player_selection(), TitleField::Id), id);
        // The reinserted selection is a separate catalog owner.
        native_title_details_release();
        native_title_player_release();
        assert!(weak.upgrade().is_some());
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(
        weak.upgrade().is_none(),
        "held UI selections leaked a title"
    );
}

#[test]
fn undo_owns_the_original_title_until_execution_and_runs_only_once() {
    let id = format!("kitsu:{}", "complete-id".repeat(500));
    let title = snapshot(&id, "poster");
    let weak = Arc::downgrade(&title);
    unsafe {
        native_title_reset_undo();
        let source = native_title_create(Arc::into_raw(title).cast());
        native_title_queue_undo(source, 1, 0);
        native_title_destroy(source);
        native_title_frame();
        native_title_frame();
        assert_eq!(action_notice_has_undo(), 1);
        assert!(weak.upgrade().is_some());
        // Closing with OK preserves the action for reopening its island notice.
        action_notice_button(2);
        assert_eq!(action_notice_has_undo(), 1);
        action_notice_button(1);
        assert_eq!(CStr::from_ptr(native_title_undo_id()).to_str().unwrap(), id);
        assert_eq!(native_title_undo_intent(), 1);
        assert_eq!(native_title_undo_executions(), 1);
        action_notice_button(1);
        assert_eq!(native_title_undo_executions(), 1);
        assert_eq!(action_notice_has_undo(), 0);
        native_title_reset_undo();
    }
    assert!(weak.upgrade().is_none(), "executed undo leaked its title");
}

#[test]
fn replacing_or_cancelling_undo_releases_its_title_and_clone_failure_offers_no_undo() {
    let first = snapshot("first", "poster");
    let first_weak = Arc::downgrade(&first);
    let second = snapshot("second", "poster");
    let second_weak = Arc::downgrade(&second);
    unsafe {
        native_title_reset_undo();
        let source = native_title_create(Arc::into_raw(first).cast());
        native_title_queue_undo(source, 1, 0);
        native_title_destroy(source);
        let next = native_title_create(Arc::into_raw(second).cast());
        native_title_queue_undo(next, 0, 0);
        native_title_destroy(next);
        assert!(first_weak.upgrade().is_none());
        assert!(second_weak.upgrade().is_some());
        native_title_reset_undo();
        assert!(second_weak.upgrade().is_none());
        let failed = snapshot("failed", "poster");
        let failed_weak = Arc::downgrade(&failed);
        let source = native_title_create(Arc::into_raw(failed).cast());
        native_title_queue_undo(source, 1, 1);
        native_title_destroy(source);
        assert_eq!(action_notice_has_undo(), 0);
        action_notice_button(1);
        assert_eq!(native_title_undo_executions(), 0);
        assert!(failed_weak.upgrade().is_none());
        native_title_reset_undo();
    }
}

#[test]
fn actual_saved_menu_and_live_channel_keep_independent_complete_selections() {
    let id = format!("kitsu:{}", "full-saved-id".repeat(400));
    let title = snapshot(&id, "poster");
    let weak = Arc::downgrade(&title);
    unsafe {
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        let source = native_title_create(Arc::into_raw(title).cast());
        native_title_open_saved_menu(source);
        native_title_set_channel(source);
        native_title_destroy(source);
        assert_eq!(get(native_title_menu_selection(), TitleField::Id), id);
        assert_eq!(
            CStr::from_ptr(native_title_channel_id()).to_str().unwrap(),
            id
        );
        // Reopening for another title releases only the menu's previous owner.
        let next = native_title_create(Arc::into_raw(snapshot("kitsu:next", "poster")).cast());
        native_title_open_saved_menu(next);
        native_title_destroy(next);
        assert_eq!(
            get(native_title_menu_selection(), TitleField::Id),
            "kitsu:next"
        );
        assert!(weak.upgrade().is_some());
        native_title_menu_release();
        native_title_player_release();
        assert_eq!(CStr::from_ptr(native_title_channel_id()).to_bytes(), b"");
    }
    assert!(
        weak.upgrade().is_none(),
        "menu/channel selection leaked its owner"
    );
}

#[test]
fn retained_catalog_detail_and_partial_metadata_paths_use_rust_rating_conversion() {
    for (rating, expected, partial) in [
        (json!("8.5"), 85, 85),
        (json!(10), 100, 100),
        (json!("10.0"), 100, 100),
        (json!(9.95), 100, 100),
        (json!(9.94), 99, 99),
        (json!(100), 100, 100),
        (json!(85), 85, 85),
        (json!(0.01), 0, 0),
        (json!(null), 0, 85),
        (json!("NaN"), 0, 85),
        (json!("1e400"), 0, 85),
        (json!(101), 0, 85),
    ] {
        let body = CString::new(
            json!({"id":"tt1","name":"Title","poster":"poster","imdbRating":rating}).to_string(),
        )
        .unwrap();
        let mut scores = [-1; 3];
        assert_eq!(
            unsafe { discovery_test_rating_paths(body.as_ptr(), scores.as_mut_ptr()) },
            1
        );
        assert_eq!(scores, [expected, expected, partial], "rating {rating}");
    }
}
