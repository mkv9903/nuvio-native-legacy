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
