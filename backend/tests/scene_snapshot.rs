mod support;
use nuvio_backend::{
    catalog_scene::{CatalogRowState, CatalogScene},
    content_context::ContentScope,
    error::Error,
    model::ProfileId,
    scene_cache::SceneCache,
    scene_snapshot::*,
    title_snapshot::*,
};
use std::{
    ffi::CStr,
    fs, ptr,
    sync::{
        atomic::{AtomicUsize, Ordering},
        Arc,
    },
};
use support::{scene::complete_scene, Directory};

unsafe fn title_text(title: *const TitleSnapshot, field: TitleField) -> String {
    unsafe { CStr::from_ptr(nuvio_title_text(title, field as u32)) }
        .to_str()
        .unwrap()
        .into()
}
unsafe fn row_text(row: *const RowSnapshot, field: RowField) -> String {
    unsafe { CStr::from_ptr(nuvio_row_text(row, field as u32)) }
        .to_str()
        .unwrap()
        .into()
}
fn scope() -> ContentScope {
    ContentScope {
        member: "member".into(),
        owner: "owner".into(),
        profile: ProfileId::new(2).unwrap(),
    }
}

#[test]
fn restored_native_titles_preserve_every_scene_field_and_unbounded_cast_and_seasons() {
    let expected = complete_scene();
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(), &expected, &|| true).unwrap();
    let snapshot = Arc::new(
        SceneSnapshot::restore(&cache, &scope(), &|| true)
            .unwrap()
            .unwrap(),
    );
    assert_eq!(snapshot.to_scene(), expected);
    let weak = Arc::downgrade(&snapshot.titles[0]);
    let scene = Arc::into_raw(snapshot);
    unsafe {
        let title = nuvio_scene_title(scene, 0);
        nuvio_scene_release(scene);
        let source = &expected.titles[0];
        for (field, value) in [
            (TitleField::Id, source.metadata.id.as_str()),
            (TitleField::ImdbId, "tt1234567"),
            (TitleField::Poster, source.metadata.poster.as_str()),
            (TitleField::CatalogBackground, "https://art.invalid/catalog"),
            (TitleField::TmdbBackground, "https://art.invalid/tmdb"),
            (TitleField::TraktBackground, "https://art.invalid/trakt"),
            (TitleField::LogoLanguage, "en"),
            (TitleField::LogoLanguageUrl, "https://art.invalid/logo"),
            (TitleField::GenreText, "Drama · Adventure"),
            (TitleField::MetadataText, "2026 · 80 seasons"),
            (TitleField::Classification, "TV-14"),
            (TitleField::Director, source.display.director.as_str()),
            (TitleField::SubscriptionName, "Subscription"),
            (TitleField::SubscriptionLogo, "subscription-logo"),
            (TitleField::RentalName, "Rental"),
            (TitleField::RentalLogo, "rental-logo"),
            (TitleField::PurchaseName, "Purchase"),
            (TitleField::PurchaseLogo, "purchase-logo"),
            (TitleField::EpisodeName, source.resume.episode_name.as_str()),
        ] {
            assert_eq!(title_text(title, field), value);
        }
        assert_eq!(nuvio_title_rating(title), 100);
        assert_eq!(nuvio_title_tmdb_id(title), 987);
        let info = &*nuvio_title_info(title);
        assert_eq!(
            (
                info.percentage,
                info.season,
                info.episode,
                info.remaining_minutes
            ),
            (47, 5, 13, 24)
        );
        assert_eq!(info.last_watched_ms, 1_234_567_890_123);
        assert_eq!(
            (
                info.on_watchlist,
                info.in_collection,
                info.cast_count,
                info.season_count
            ),
            (1, 1, 25, 80)
        );
        for (index, member) in source.cast.iter().enumerate() {
            for (field, expected) in [
                member.name.as_str(),
                member.role.as_str(),
                member.photo.as_str(),
            ]
            .into_iter()
            .enumerate()
            {
                assert_eq!(
                    CStr::from_ptr(nuvio_title_cast_text(title, index, field as u32))
                        .to_str()
                        .unwrap(),
                    expected
                );
            }
            assert_eq!(
                nuvio_title_cast_tmdb_id(title, index),
                member.tmdb_id.unwrap()
            );
        }
        for (index, season) in source.seasons.iter().enumerate() {
            assert_eq!(nuvio_title_season(title, index), *season);
        }
        nuvio_title_release(title);
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn retained_row_outlives_scene_and_keeps_full_configured_and_encoded_requests() {
    let expected = complete_scene();
    let source = expected.rows[0].source.as_ref().unwrap();
    let request = source
        .addon
        .resource_url_with_extras(
            "catalog",
            &source.media_type,
            &source.catalog_id,
            &[("genre", "Sci-Fi 日"), ("skip", "24")],
        )
        .unwrap();
    let snapshot = Arc::new(SceneSnapshot::new(expected.clone()).unwrap());
    let weak = Arc::downgrade(&snapshot.rows[0]);
    let scene = Arc::into_raw(snapshot);
    unsafe {
        let row = nuvio_scene_row(scene, 0);
        let retained = nuvio_row_retain(row);
        let borrowed = nuvio_row_text(row, RowField::Key as u32);
        nuvio_scene_release(scene);
        nuvio_row_release(row);
        assert_eq!(
            CStr::from_ptr(borrowed).to_str().unwrap(),
            expected.rows[0].key
        );
        assert_eq!(
            row_text(retained, RowField::AddonUrl),
            source.addon.as_str()
        );
        assert_eq!(row_text(retained, RowField::CatalogId), source.catalog_id);
        assert_eq!(row_text(retained, RowField::RequestUrl), request.as_str());
        assert!(request.as_str().contains("private=keep"));
        let info = &*nuvio_row_info(retained);
        assert_eq!((info.start, info.end, info.state), (0, 1, 1));
        nuvio_row_release(retained);
    }
    assert!(weak.upgrade().is_none());
}

#[test]
fn explicit_loading_empty_rows_and_empty_scenes_stay_distinct_and_bad_windows_are_rejected() {
    let mut empty = complete_scene();
    empty.titles.clear();
    empty.rows[0].titles = 0..0;
    empty.rows[0].state = CatalogRowState::ConfirmedEmpty;
    unsafe {
        let scene = native_scene(empty.clone()).unwrap();
        let row = nuvio_scene_row(scene, 0);
        assert_eq!(nuvio_scene_title_count(scene), 0);
        assert_eq!((*nuvio_row_info(row)).state, 2);
        nuvio_row_release(row);
        nuvio_scene_release(scene);
        empty.rows[0].state = CatalogRowState::Loading;
        let scene = native_scene(empty.clone()).unwrap();
        let row = nuvio_scene_row(scene, 0);
        assert_eq!((*nuvio_row_info(row)).state, 0);
        nuvio_row_release(row);
        nuvio_scene_release(scene);
        let scene = native_scene(CatalogScene::default()).unwrap();
        assert_eq!(nuvio_scene_row_count(scene), 0);
        nuvio_scene_release(scene);
    }
    empty.rows[0].titles = 0..1;
    assert!(matches!(SceneSnapshot::new(empty), Err(Error::InvalidData)));
}

#[test]
fn native_scene_restore_ignores_original_and_discarded_storage_and_foreign_app_directories() {
    let directory = Directory::new();
    for name in [
        "catalogo-rede.bin",
        "sessao.txt",
        "session.json",
        "settings.json",
    ] {
        fs::write(directory.path.join(name), b"original or discarded storage").unwrap();
    }
    let cache = SceneCache::open(&directory.path).unwrap();
    assert!(SceneSnapshot::restore(&cache, &scope(), &|| true)
        .unwrap()
        .is_none());
    let foreign = directory
        .path
        .parent()
        .unwrap()
        .join("space.mkv.nuvio.native");
    fs::create_dir(&foreign).unwrap();
    assert!(matches!(
        SceneCache::open(foreign),
        Err(Error::ProductionData)
    ));
    let expected = complete_scene();
    cache.save(&scope(), &expected, &|| true).unwrap();
    assert_eq!(
        SceneSnapshot::restore(&cache, &scope(), &|| true)
            .unwrap()
            .unwrap()
            .to_scene(),
        expected
    );
}

#[test]
fn context_change_after_json_restore_cancels_native_projection() {
    let directory = Directory::new();
    let cache = SceneCache::open(&directory.path).unwrap();
    cache.save(&scope(), &complete_scene(), &|| true).unwrap();
    let checks = AtomicUsize::new(0);
    let result = SceneSnapshot::restore(&cache, &scope(), &|| {
        checks.fetch_add(1, Ordering::SeqCst) < 3
    });
    assert!(matches!(result, Err(Error::Cancelled)));
    assert_eq!(checks.load(Ordering::SeqCst), 4);
}

#[test]
fn empty_and_out_of_range_native_queries_are_safe_and_display_projections_are_utf8_prefixes() {
    unsafe {
        assert!(nuvio_scene_title(ptr::null(), 0).is_null());
        assert!(nuvio_scene_row(ptr::null(), 0).is_null());
        assert!(nuvio_title_info(ptr::null()).is_null());
        assert!(nuvio_row_info(ptr::null()).is_null());
        assert_eq!(nuvio_title_cast_tmdb_id(ptr::null(), 0), 0);
        assert_eq!(nuvio_title_season(ptr::null(), 0), 0);
        nuvio_scene_release(ptr::null());
        nuvio_row_release(ptr::null());
        let scene = native_scene(complete_scene()).unwrap();
        let title = nuvio_scene_title(scene, 0);
        assert!(nuvio_scene_title(scene, 1).is_null());
        assert!(nuvio_scene_row(scene, 1).is_null());
        assert_eq!(
            CStr::from_ptr(nuvio_title_cast_text(title, 25, 0)).to_bytes(),
            b""
        );
        assert_eq!(
            CStr::from_ptr(nuvio_title_cast_text(title, 0, 3)).to_bytes(),
            b""
        );
        let mut bytes = [0 as std::ffi::c_char; 12];
        nuvio_title_copy_cast_text(title, 0, 0, bytes.as_mut_ptr(), 10);
        assert_eq!(CStr::from_ptr(bytes.as_ptr()).to_str().unwrap(), "Actor 0 ");
        nuvio_title_release(title);
        nuvio_scene_release(scene);
    }
}
