#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    catalog_scene::{CastMember, SceneTitle},
    catalog_titles::CatalogTitle,
    title_snapshot::TitleSnapshot,
};
use serde_json::json;
use std::{
    ffi::{c_char, c_void, CStr, CString},
    sync::Arc,
};
use support::Directory;
#[link(name = "nuvio_c_ui")]
extern "C" {
    fn data_init(directory: *const c_char);
    fn ajustes_iniciar();
    fn native_title_create(title: *const c_void) -> *mut c_void;
    fn native_title_destroy(title: *mut c_void);
    fn native_title_cast_projection(
        title: *const c_void,
        index: usize,
        field: u32,
    ) -> *const c_char;
    fn native_title_frame();
    fn native_search_test_seed(title: *const c_void);
    fn search_screen_test_query(query: *const c_char);
    fn search_screen_test_row_count() -> i32;
    fn search_screen_test_select_first() -> i32;
    fn quick_search_test_query(query: *const c_char);
    fn spot_n_linhas() -> i32;
    fn spot_linha_texto(index: i32) -> *const c_char;
}
#[test]
fn both_search_interfaces_omit_people_and_still_open_titles_without_losing_cast() {
    let directory = Directory::new();
    std::env::set_var("NUVIO_DADOS", &directory.parent);
    let mut title = SceneTitle::from(
        CatalogTitle::from_meta(
            &json!({
                "id":"tt0133093", "name":"The Matrix", "poster":"https://art.invalid/poster"
            }),
            "movie",
        )
        .unwrap(),
    );
    title.cast = vec![CastMember {
        name: "Christian Bale".into(),
        role: "Actor".into(),
        photo: String::new(),
        tmdb_id: Some(3894),
    }];
    let title = Arc::new(TitleSnapshot::from_scene(title));
    let weak = Arc::downgrade(&title);
    unsafe {
        data_init(std::ptr::null());
        ajustes_iniciar();
        let native = native_title_create(Arc::into_raw(title).cast());
        native_search_test_seed(native);
        let actor = CString::new("chri").unwrap();
        search_screen_test_query(actor.as_ptr());
        assert_eq!(
            search_screen_test_row_count(),
            0,
            "loaded cast cannot become Search results"
        );
        quick_search_test_query(actor.as_ptr());
        for row in 0..spot_n_linhas() {
            let text = CStr::from_ptr(spot_linha_texto(row)).to_str().unwrap();
            assert!(
                !text.contains("Christian Bale"),
                "People cannot appear in quick search"
            );
        }
        let movie = CString::new("matrix").unwrap();
        search_screen_test_query(movie.as_ptr());
        assert_eq!(search_screen_test_row_count(), 1);
        assert_eq!(search_screen_test_select_first(), 0);
        quick_search_test_query(movie.as_ptr());
        assert!(
            (0..spot_n_linhas()).any(|row| CStr::from_ptr(spot_linha_texto(row))
                .to_string_lossy()
                .contains("The Matrix"))
        );
        assert_eq!(
            CStr::from_ptr(native_title_cast_projection(native, 0, 0))
                .to_str()
                .unwrap(),
            "Christian Bale"
        );
        native_title_destroy(native);
        native_search_test_seed(std::ptr::null());
        native_search_test_seed(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn both_native_search_surfaces_read_the_selected_profiles_rust_history() {
    use nuvio_backend::{
        content_context::ContentScope,
        model::ProfileId,
        search_history::SearchHistoryStore,
        search_history_runtime::{HistoryAccount, SearchHistoryRuntime},
    };
    use std::{
        sync::Mutex,
        thread,
        time::{Duration, Instant},
    };
    struct Account(Mutex<Option<ContentScope>>);
    impl HistoryAccount for Account {
        fn history_scope(&self) -> Option<ContentScope> {
            self.0.lock().unwrap().clone()
        }
    }
    extern "C" {
        fn search_history_test_attach(runtime: *const c_void);
        fn search_history_count() -> i32;
        fn search_history_query(index: i32) -> *const c_char;
        fn search_history_record(query: *const c_char) -> i32;
        fn search_history_remove(index: i32);
        fn search_history_clear();
        fn search_history_logout();
    }
    let directory = Directory::new();
    let first = ContentScope {
        member: "native-member".into(),
        owner: "native-owner".into(),
        profile: ProfileId::new(1).unwrap(),
    };
    let account = Arc::new(Account(Mutex::new(Some(first.clone()))));
    let store = Arc::new(SearchHistoryStore::open(&directory.path).unwrap());
    let runtime = SearchHistoryRuntime::new(account.clone(), store.clone()).unwrap();
    struct Attachment;
    impl Drop for Attachment {
        fn drop(&mut self) {
            unsafe {
                search_history_test_attach(std::ptr::null());
            }
        }
    }
    let attachment = Attachment;
    unsafe {
        search_history_test_attach((&runtime as *const SearchHistoryRuntime).cast());
        assert_eq!(search_history_record(c"Matrix".as_ptr()), 1);
        assert_eq!(search_history_count(), 1);
        quick_search_test_query(c"".as_ptr());
        assert!((0..spot_n_linhas())
            .any(|row| CStr::from_ptr(spot_linha_texto(row)).to_bytes() == b"Matrix"));
        let full = CString::new("Complete query 日 ".repeat(100).trim_end()).unwrap();
        assert_eq!(search_history_record(full.as_ptr()), 1);
        assert_eq!(CStr::from_ptr(search_history_query(0)), full.as_c_str());
        let mut second = first.clone();
        second.profile = ProfileId::new(2).unwrap();
        *account.0.lock().unwrap() = Some(second.clone());
        assert_eq!(
            search_history_count(),
            0,
            "a profile switch must hide the previous queries immediately"
        );
        search_history_record(c"Second profile".as_ptr());
        quick_search_test_query(c"".as_ptr());
        assert!((0..spot_n_linhas())
            .any(|row| CStr::from_ptr(spot_linha_texto(row)).to_bytes() == b"Second profile"));
        assert!((0..spot_n_linhas())
            .all(|row| CStr::from_ptr(spot_linha_texto(row)).to_bytes() != b"Matrix"));
        *account.0.lock().unwrap() = Some(first.clone());
        assert_eq!(search_history_count(), 2);
        search_history_remove(0);
        assert_eq!(search_history_count(), 1);
        assert_eq!(CStr::from_ptr(search_history_query(0)), c"Matrix");
        search_history_clear();
        assert_eq!(search_history_count(), 0);
        *account.0.lock().unwrap() = None;
        search_history_logout();
        let deadline = Instant::now() + Duration::from_secs(5);
        while !store.load(&second, &|| true).unwrap().queries().is_empty() {
            assert!(Instant::now() < deadline);
            thread::sleep(Duration::from_millis(5));
        }
        search_history_test_attach(std::ptr::null());
    }
    drop(attachment);
    drop(runtime);
    assert!(store.load(&first, &|| true).unwrap().queries().is_empty());
}
