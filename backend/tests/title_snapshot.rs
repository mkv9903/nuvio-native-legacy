use nuvio_backend::title_snapshot::*;
use serde_json::json;
use std::{
    ffi::{c_char, CStr, CString},
    sync::Arc,
    thread,
};

fn decode(body: serde_json::Value) -> *const TitleSnapshot {
    let body = CString::new(body.to_string()).unwrap();
    unsafe { nuvio_title_decode(body.as_ptr(), c"movie".as_ptr(), c"provider".as_ptr()) }
}
#[test]
fn native_titles_keep_provider_identity_separate_and_own_full_values() {
    let id = format!("kitsu:{}", "identity".repeat(600));
    let image = format!("https://art.invalid/{}", "configured-path".repeat(800));
    let name = "A complete title 日".repeat(300);
    let title = decode(
        json!({"id":id,"imdb_id":"tt1234567","name":name,"poster":image,
        "type":"series","genres":["Drama","Adventure"],"releaseInfo":"2026–","runtime":"148m","imdbRating":"8.5","moviedb_id":987}),
    );
    assert!(!title.is_null());
    let held = unsafe { nuvio_title_retain(title) };
    let borrowed = unsafe { nuvio_title_text(title, TitleField::Poster as u32) };
    unsafe {
        nuvio_title_release(title);
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(held, TitleField::Id as u32))
                .to_str()
                .unwrap(),
            id
        );
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(held, TitleField::ImdbId as u32))
                .to_str()
                .unwrap(),
            "tt1234567"
        );
        assert_eq!(CStr::from_ptr(borrowed).to_str().unwrap(), image);
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(held, TitleField::Name as u32))
                .to_str()
                .unwrap(),
            name
        );
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(held, TitleField::GenreText as u32))
                .to_str()
                .unwrap(),
            "TV Show  ·  Drama  ·  Adventure"
        );
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(held, TitleField::MetadataText as u32))
                .to_str()
                .unwrap(),
            "2026  ·  148 min"
        );
        assert_eq!(nuvio_title_rating(held), 85);
        assert_eq!(nuvio_title_tmdb_id(held), 987);
        nuvio_title_release(held);
    }
}
#[test]
fn projections_stop_at_utf8_boundaries_without_losing_owned_text() {
    let title = decode(json!({"id":"id","name":"日AB","poster":"poster"}));
    let mut bytes = [99 as c_char; 8];
    unsafe {
        nuvio_title_copy_text(title, TitleField::Name as u32, bytes.as_mut_ptr(), 3);
        assert_eq!(&bytes[..4], [0, 99, 99, 99]);
        nuvio_title_copy_text(title, TitleField::Name as u32, bytes.as_mut_ptr(), 4);
        assert_eq!(CStr::from_ptr(bytes.as_ptr()).to_str().unwrap(), "日");
        assert_eq!(bytes[4], 99);
        nuvio_title_copy_text(title, TitleField::Name as u32, bytes.as_mut_ptr(), 0);
        assert_eq!(bytes[4], 99);
        nuvio_title_copy_text(title, TitleField::Name as u32, std::ptr::null_mut(), 1);
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(title, TitleField::Name as u32))
                .to_str()
                .unwrap(),
            "日AB"
        );
        nuvio_title_release(title);
    }
}
#[test]
fn null_handles_bad_inputs_and_display_nul_are_safe() {
    unsafe {
        assert!(
            nuvio_title_decode(std::ptr::null(), c"movie".as_ptr(), c"provider".as_ptr()).is_null()
        );
        assert!(
            nuvio_title_decode(c"{}".as_ptr(), c"movie".as_ptr(), c"provider".as_ptr()).is_null()
        );
        assert!(
            nuvio_title_decode(c"null".as_ptr(), c"movie".as_ptr(), c"provider".as_ptr()).is_null()
        );
        assert!(nuvio_title_retain(std::ptr::null()).is_null());
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(std::ptr::null(), 0)).to_bytes(),
            b""
        );
        nuvio_title_release(std::ptr::null());
        let title = decode(json!({"id":"id","name":"A\u{0}B","poster":"poster"}));
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(title, TitleField::Name as u32)).to_bytes(),
            b"A B"
        );
        assert_eq!(
            CStr::from_ptr(nuvio_title_text(title, 9999)).to_bytes(),
            b""
        );
        nuvio_title_release(title);
    }
}
#[test]
fn concurrent_native_readers_keep_the_snapshot_alive_after_creator_release() {
    let title = decode(json!({"id":"provider-id","name":"Title","poster":"poster"}));
    let owned = unsafe { Arc::from_raw(title) };
    thread::scope(|scope| {
        for _ in 0..8 {
            let reader = owned.clone();
            scope.spawn(move || {
                let ptr = Arc::into_raw(reader);
                for _ in 0..1000 {
                    assert_eq!(
                        unsafe { CStr::from_ptr(nuvio_title_text(ptr, TitleField::Id as u32)) }
                            .to_bytes(),
                        b"provider-id"
                    );
                }
                unsafe {
                    nuvio_title_release(ptr);
                }
            });
        }
        drop(owned);
    });
}
#[test]
fn ratings_keep_their_scale_when_perfect_or_rounded_to_one_hundred_percent() {
    for (rating, expected) in [
        (json!("8.5"), 85),
        (json!(10), 100),
        (json!("10.0"), 100),
        (json!(9.95), 100),
        (json!(9.94), 99),
        (json!(100), 100),
        (json!(99.5), 100),
        (json!(null), 0),
    ] {
        let title = decode(json!({"id":"id","name":"Title","poster":"poster","imdbRating":rating}));
        assert!(!title.is_null());
        unsafe {
            assert_eq!(nuvio_title_rating(title), expected, "rating {rating}");
            let value = rating
                .as_f64()
                .or_else(|| rating.as_str()?.parse().ok())
                .unwrap_or(0.0);
            assert_eq!(nuvio_title_rating_value(value), expected);
            nuvio_title_release(title);
        }
    }
    for invalid in [f64::NAN, f64::INFINITY, f64::NEG_INFINITY, -1.0, 101.0] {
        assert_eq!(nuvio_title_rating_value(invalid), 0);
    }
}
