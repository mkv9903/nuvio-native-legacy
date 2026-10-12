use nuvio_backend::{
    addon_snapshot::*,
    addons::{AddonUrl, InstalledAddon, Manifest},
    ffi::nuvio_text_destroy,
};
use serde_json::json;
use std::{
    ffi::{CStr, CString},
    ptr,
    sync::Arc,
};

fn snapshot() -> NativeAddonSnapshot {
    let manifest=Arc::new(Manifest::decode(&serde_json::to_vec(&json!({"id":"root-addon","name":"Root name","resources":[{"name":"meta","types":["series"]},"stream"],"idPrefixes":["tt"],"catalogs":(0..703).map(|index|json!({"id":format!("c{index}"),"type":"movie","name":format!("Catalog {index}")})).collect::<Vec<_>>()})).unwrap()).unwrap());
    let records: Arc<[_]> = (0..80)
        .map(|index| AddonRecord {
            installed: InstalledAddon {
                name: format!("{} {index}", "Long display name".repeat(20)),
                url: AddonUrl::parse(&format!(
                    "https://addon{index}.invalid/{}?key=keep/",
                    "config".repeat(700)
                ))
                .unwrap(),
                enabled: index != 79,
            },
            manifest: if index == 0 {
                Some(manifest.clone())
            } else {
                None
            },
            silent_queries: 2,
        })
        .collect::<Vec<_>>()
        .into();
    NativeAddonSnapshot::new(AddonSnapshot {
        context: None,
        revision: 7,
        list_revision: 7,
        records,
        authoritative: true,
        pending: false,
    })
}

#[test]
fn native_views_preserve_large_lists_full_strings_and_late_catalogs() {
    let snapshot = snapshot();
    let view = snapshot.view();
    assert_eq!(view.count, 80);
    assert_eq!(view.revision, 7);
    unsafe {
        let first = &*view.addons;
        let last = &*view.addons.add(79);
        assert_eq!(CStr::from_ptr(first.name).to_str().unwrap(), "Root name");
        assert_eq!(first.catalog_count, 703);
        assert_eq!(
            CStr::from_ptr((*first.catalogs.add(702)).id)
                .to_str()
                .unwrap(),
            "c702"
        );
        assert!(CStr::from_ptr(last.base).to_bytes().len() > 2047);
        assert!(CStr::from_ptr(last.name).to_bytes().len() > 64);
        assert_eq!(last.enabled, 0);
        assert_eq!(last.known, 0);
        assert_eq!(last.subtitles, 1);
        assert_eq!(last.metadata, 0);
    }
}

#[test]
fn native_workers_share_immutable_views_after_the_parent_is_destroyed() {
    let parent = Box::into_raw(Box::new(snapshot()));
    let children: Vec<_> = (0..12)
        .map(|_| unsafe { nuvio_addons_snapshot_clone(parent) } as usize)
        .collect();
    unsafe {
        nuvio_addons_snapshot_destroy(parent);
    }
    std::thread::scope(|threads| {
        for child in children {
            threads.spawn(move || unsafe {
                let child = child as *mut NativeAddonSnapshot;
                let view = nuvio_addons_snapshot_view(child);
                assert_eq!((*view).count, 80);
                assert_eq!(
                    CStr::from_ptr((*(*view).addons).id).to_str().unwrap(),
                    "root-addon"
                );
                assert_eq!((*(*view).addons).catalog_count, 703);
                nuvio_addons_snapshot_destroy(child);
            });
        }
    });
}

#[test]
fn native_resource_filters_obey_independent_manifest_definitions() {
    let snapshot = snapshot();
    let series = CString::new("series").unwrap();
    let movie = CString::new("movie").unwrap();
    let id = CString::new("kitsu:123").unwrap();
    unsafe {
        assert_eq!(
            nuvio_addons_snapshot_accepts(&snapshot, 0, 3, series.as_ptr(), id.as_ptr()),
            1
        );
        assert_eq!(
            nuvio_addons_snapshot_accepts(&snapshot, 0, 3, movie.as_ptr(), id.as_ptr()),
            0
        );
        assert_eq!(
            nuvio_addons_snapshot_accepts(&snapshot, 1, 3, series.as_ptr(), id.as_ptr()),
            -1
        );
        assert_eq!(
            nuvio_addons_snapshot_accepts(&snapshot, 80, 3, series.as_ptr(), id.as_ptr()),
            0
        );
        assert_eq!(
            nuvio_addons_snapshot_accepts(&snapshot, 0, 99, series.as_ptr(), id.as_ptr()),
            0
        );
        assert_eq!(
            nuvio_addons_snapshot_accepts(ptr::null(), 0, 3, series.as_ptr(), id.as_ptr()),
            0
        );
    }
}

#[test]
fn generated_source_urls_are_owned_complete_and_encoded_after_snapshot_release() {
    let snapshot = Box::into_raw(Box::new(snapshot()));
    let resource = CString::new("stream").unwrap();
    let kind = CString::new("series").unwrap();
    let id = CString::new("kitsu:123:1:2 /日?#%").unwrap();
    unsafe {
        let url =
            nuvio_addons_snapshot_url(snapshot, 79, resource.as_ptr(), kind.as_ptr(), id.as_ptr());
        assert!(!url.is_null());
        nuvio_addons_snapshot_destroy(snapshot);
        let complete = CStr::from_ptr(url).to_str().unwrap();
        assert!(complete.len() > 4096);
        assert!(complete.starts_with("https://addon79.invalid/config"));
        assert!(complete.ends_with(".json?key=keep/"));
        assert!(complete.contains("%2F%E6%97%A5%3F%23%25"));
        nuvio_text_destroy(url);
        assert!(nuvio_addons_snapshot_url(
            ptr::null(),
            0,
            resource.as_ptr(),
            kind.as_ptr(),
            id.as_ptr()
        )
        .is_null());
        assert!(nuvio_addons_snapshot_clone(ptr::null()).is_null());
        assert!(nuvio_addons_snapshot_view(ptr::null()).is_null());
        nuvio_addons_snapshot_destroy(ptr::null_mut());
    }
}

#[test]
fn display_nul_is_sanitized_without_truncating_valid_unicode() {
    let record = AddonRecord {
        installed: InstalledAddon {
            name: "日 \0 long".into(),
            url: AddonUrl::parse("https://addon.invalid").unwrap(),
            enabled: true,
        },
        manifest: None,
        silent_queries: 0,
    };
    let snapshot = NativeAddonSnapshot::new(AddonSnapshot {
        context: None,
        revision: 0,
        list_revision: 0,
        records: vec![record].into(),
        authoritative: false,
        pending: true,
    });
    assert_eq!(
        unsafe { CStr::from_ptr((*snapshot.view().addons).name) }
            .to_str()
            .unwrap(),
        "日   long"
    );
}
