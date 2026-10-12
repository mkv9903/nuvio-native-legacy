mod support;
use nuvio_backend::{
    addon_storage::{AddonEdit, AddonStorage, AddonStore},
    addons::{AddonUrl, InstalledAddon},
    content_context::AddonScope,
    error::Error,
    model::ProfileId,
};
use serde_json::json;
use std::{fs, sync::Arc};
use support::Directory;
use uuid::Uuid;

fn scope(member: &str, owner: &str, profile: u32) -> AddonScope {
    AddonScope {
        member: member.into(),
        owner: owner.into(),
        profile: ProfileId::new(profile).unwrap(),
    }
}
fn addon(name: &str) -> InstalledAddon {
    InstalledAddon {
        name: name.into(),
        url: AddonUrl::parse("https://addon.invalid/config").unwrap(),
        enabled: false,
    }
}
fn payload_file(directory: &Directory, prefix: &str) -> std::path::PathBuf {
    fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| {
            path.file_name()
                .unwrap()
                .to_str()
                .unwrap()
                .starts_with(prefix)
        })
        .unwrap()
}

#[test]
fn fresh_cache_and_edits_round_trip_large_lists_long_urls_and_empty_names() {
    let directory = Directory::new();
    let identity = scope("member", "shared-owner", 3);
    let store = AddonStore::open(&directory.path).unwrap();
    assert!(store.load_cache(&identity).unwrap().is_none());
    assert!(store.load_edit(&identity).unwrap().is_none());
    let mut list: Vec<_> = (0..80)
        .map(|index| addon(&format!("Addon {index}")))
        .collect();
    list[79].name = String::new();
    list[79].url = AddonUrl::parse(&format!(
        "https://addon.invalid/{}/manifest.json",
        "configuration".repeat(600)
    ))
    .unwrap();
    let edit = AddonEdit {
        id: Uuid::new_v4(),
        addons: list.clone(),
    };
    store.save_cache(&identity, &list).unwrap();
    store.save_edit(&identity, &edit).unwrap();
    drop(store);
    let restored = AddonStore::open(&directory.path).unwrap();
    assert_eq!(restored.load_cache(&identity).unwrap().unwrap(), list);
    assert_eq!(restored.load_edit(&identity).unwrap().unwrap(), edit);
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        for file in fs::read_dir(directory.path.join("rust-backend-v1")).unwrap() {
            assert_eq!(
                file.unwrap().metadata().unwrap().permissions().mode() & 0o777,
                0o600
            );
        }
    }
}

#[test]
fn account_owner_and_profile_are_isolated_and_paths_never_embed_private_values() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    let original = scope("member/private? & 日", "owner/with/query?secret=private", 2);
    store.save_cache(&original, &[addon("Original")]).unwrap();
    store
        .save_edit(
            &original,
            &AddonEdit {
                id: Uuid::new_v4(),
                addons: vec![addon("Edit")],
            },
        )
        .unwrap();
    for other in [
        scope("different-member", &original.owner, 2),
        scope(&original.member, "different-owner", 2),
        scope(&original.member, &original.owner, 3),
    ] {
        assert!(store.load_cache(&other).unwrap().is_none());
        assert!(store.load_edit(&other).unwrap().is_none());
    }
    for entry in fs::read_dir(directory.path.join("rust-backend-v1")).unwrap() {
        let name = entry.unwrap().file_name();
        let name = name.to_str().unwrap();
        assert!(!name.contains("private"));
        assert!(name.len() < 100);
    }
}

#[test]
fn confirmed_empty_cache_and_empty_pending_removal_are_valid() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    let identity = scope("member", "owner", 1);
    store.save_cache(&identity, &[]).unwrap();
    let edit = AddonEdit {
        id: Uuid::new_v4(),
        addons: Vec::new(),
    };
    store.save_edit(&identity, &edit).unwrap();
    assert_eq!(store.load_cache(&identity).unwrap(), Some(Vec::new()));
    assert_eq!(store.load_edit(&identity).unwrap(), Some(edit.clone()));
    assert!(store.acknowledge(&identity, edit.id).unwrap());
    assert!(store.load_edit(&identity).unwrap().is_none());
    assert_eq!(store.load_cache(&identity).unwrap(), Some(Vec::new()));
}

#[test]
fn acknowledgement_promotes_the_edit_to_cache_before_removing_the_journal() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    let identity = scope("member", "owner", 1);
    store.save_cache(&identity, &[addon("Before")]).unwrap();
    let edit = AddonEdit {
        id: Uuid::new_v4(),
        addons: vec![addon("After")],
    };
    store.save_edit(&identity, &edit).unwrap();
    assert!(store.acknowledge(&identity, edit.id).unwrap());
    drop(store);
    let restored = AddonStore::open(&directory.path).unwrap();
    assert!(restored.load_edit(&identity).unwrap().is_none());
    assert_eq!(restored.load_cache(&identity).unwrap(), Some(edit.addons));
}

#[test]
fn paired_restore_never_observes_old_cache_with_an_acknowledged_journal_missing() {
    let directory = Directory::new();
    let writer = AddonStore::open(&directory.path).unwrap();
    let reader = AddonStore::open(&directory.path).unwrap();
    let identity = scope("member", "owner", 1);
    writer.save_cache(&identity, &[addon("Before")]).unwrap();
    let edit = AddonEdit {
        id: Uuid::new_v4(),
        addons: vec![addon("After")],
    };
    writer.save_edit(&identity, &edit).unwrap();
    std::thread::scope(|threads| {
        threads.spawn(|| {
            assert!(writer.acknowledge(&identity, edit.id).unwrap());
        });
        threads.spawn(|| {
            for _ in 0..500 {
                let state = reader.load_state(&identity).unwrap();
                if state.pending.is_none() {
                    assert_eq!(state.cached.as_ref().unwrap()[0].name, "After");
                } else {
                    assert_eq!(state.pending.as_ref().unwrap().addons[0].name, "After");
                }
            }
        });
    });
}

#[test]
fn old_acknowledgements_cannot_delete_a_newer_persisted_edit() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    let identity = scope("member", "owner", 1);
    let first = AddonEdit {
        id: Uuid::new_v4(),
        addons: vec![addon("First")],
    };
    let second = AddonEdit {
        id: Uuid::new_v4(),
        addons: vec![addon("Second")],
    };
    store.save_edit(&identity, &first).unwrap();
    store.save_edit(&identity, &second).unwrap();
    assert!(!store.acknowledge(&identity, first.id).unwrap());
    assert_eq!(store.load_edit(&identity).unwrap(), Some(second));
}

#[test]
fn concurrent_independent_store_handles_never_acknowledge_another_revision() {
    let directory = Directory::new();
    let identity = Arc::new(scope("member", "owner", 1));
    let writer = AddonStore::open(&directory.path).unwrap();
    let reader = AddonStore::open(&directory.path).unwrap();
    let old = Uuid::new_v4();
    writer
        .save_edit(
            &identity,
            &AddonEdit {
                id: old,
                addons: vec![addon("Old")],
            },
        )
        .unwrap();
    let fresh = AddonEdit {
        id: Uuid::new_v4(),
        addons: vec![addon("Fresh")],
    };
    writer.save_edit(&identity, &fresh).unwrap();
    std::thread::scope(|threads| {
        threads.spawn(|| {
            for _ in 0..300 {
                writer.save_edit(&identity, &fresh).unwrap();
            }
        });
        threads.spawn(|| {
            for _ in 0..300 {
                assert!(!reader.acknowledge(&identity, old).unwrap());
            }
        });
    });
    assert_eq!(writer.load_edit(&identity).unwrap(), Some(fresh));
}

#[test]
fn unknown_versions_corrupt_identity_and_invalid_pending_edits_stay_untouched() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    let identity = scope("member", "owner", 1);
    store
        .save_edit(
            &identity,
            &AddonEdit {
                id: Uuid::new_v4(),
                addons: vec![addon("Valid")],
            },
        )
        .unwrap();
    let file = payload_file(&directory, "addons-edit-");
    let original: serde_json::Value = serde_json::from_slice(&fs::read(&file).unwrap()).unwrap();
    for (field, value) in [
        ("version", json!(2)),
        ("app_id", json!("production")),
        (
            "scope",
            json!({"member":"other","owner":"owner","profile":1}),
        ),
    ] {
        let mut wrong = original.clone();
        wrong[field] = value;
        let bytes = serde_json::to_vec(&wrong).unwrap();
        fs::write(&file, &bytes).unwrap();
        assert!(store.load_edit(&identity).is_err());
        assert_eq!(fs::read(&file).unwrap(), bytes);
    }
    for body in ["{", "{}", ""] {
        fs::write(&file, body).unwrap();
        assert!(store.load_edit(&identity).is_err());
        assert_eq!(fs::read_to_string(&file).unwrap(), body);
    }
    assert_eq!(
        store.save_edit(
            &identity,
            &AddonEdit {
                id: Uuid::nil(),
                addons: Vec::new()
            }
        ),
        Err(Error::InvalidData)
    );
}

#[test]
fn legacy_files_are_ignored_and_logout_clears_only_own_payloads() {
    let directory = Directory::new();
    let store = AddonStore::open(&directory.path).unwrap();
    for name in [
        "addons.txt",
        "conta-addons-pend-member-p1.txt",
        "conta-addons-p1.txt",
    ] {
        fs::write(directory.path.join(name), "old data").unwrap();
    }
    for profile in [1, 3, 40] {
        let identity = scope("member", "owner", profile);
        assert!(store.load_edit(&identity).unwrap().is_none());
        store.save_cache(&identity, &[addon("Cached")]).unwrap();
        store
            .save_edit(
                &identity,
                &AddonEdit {
                    id: Uuid::new_v4(),
                    addons: vec![addon("Edited")],
                },
            )
            .unwrap();
    }
    store.clear().unwrap();
    for profile in [1, 3, 40] {
        let identity = scope("member", "owner", profile);
        assert!(store.load_cache(&identity).unwrap().is_none());
        assert!(store.load_edit(&identity).unwrap().is_none());
    }
    for name in [
        "addons.txt",
        "conta-addons-pend-member-p1.txt",
        "conta-addons-p1.txt",
    ] {
        assert!(directory.path.join(name).exists());
    }
}

#[test]
fn symlinked_payloads_are_rejected_and_atomic_writes_do_not_modify_the_target() {
    #[cfg(unix)]
    {
        use std::os::unix::fs::symlink;
        let directory = Directory::new();
        let store = AddonStore::open(&directory.path).unwrap();
        let identity = scope("member", "owner", 1);
        store
            .save_edit(
                &identity,
                &AddonEdit {
                    id: Uuid::new_v4(),
                    addons: vec![addon("Valid")],
                },
            )
            .unwrap();
        let file = payload_file(&directory, "addons-edit-");
        let outside = directory.parent.join("outside.json");
        fs::write(&outside, "private").unwrap();
        fs::remove_file(&file).unwrap();
        symlink(&outside, &file).unwrap();
        assert!(store.load_edit(&identity).is_err());
        assert!(store.acknowledge(&identity, Uuid::new_v4()).is_err());
        store
            .save_edit(
                &identity,
                &AddonEdit {
                    id: Uuid::new_v4(),
                    addons: vec![addon("New")],
                },
            )
            .unwrap();
        assert_eq!(fs::read_to_string(outside).unwrap(), "private");
    }
}
