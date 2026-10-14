#![cfg(feature = "native-ui-tests")]
use nuvio_backend::{
    catalog_titles::CatalogTitle,
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
    fn native_title_destroy(item: *mut c_void);
    fn native_title_text(item: *const c_void, field: u32) -> *const c_char;
    fn native_title_overlay_poster(item: *mut c_void, poster: *const c_char);
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
        assert_eq!(weak.upgrade().unwrap().title.poster, poster);
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
