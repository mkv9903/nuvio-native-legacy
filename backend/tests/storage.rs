mod support;
use nuvio_backend::{
    error::Error,
    model::{ProfileId, ProfileLock, Secret, Session},
    storage::{AccountData, AccountStore},
    TEST_APP_ID,
};
use std::fs;
use support::*;

#[test]
fn migrate_complete_and_partial_legacy_sessions_and_reject_corrupt_tokens() {
    for suffix in ["\nr\n0\n", "\nr", "\n\n0"] {
        let directory = Directory::new();
        fs::write(
            directory.path.join("sessao.txt"),
            format!("{}{suffix}", token("user", Some(4102444800), "signature")),
        )
        .unwrap();
        let store = directory.store();
        let data = store.load().unwrap();
        assert_eq!(data.session.as_ref().unwrap().subject, "user");
        assert!(!data.session.unwrap().anonymous);
        assert!(!directory.path.join("sessao.txt").exists());
        assert!(directory.path.join("account.json").exists());
        assert_eq!(store.load().unwrap().session.unwrap().subject, "user");
    }
    for text in [
        "",
        "truncated-token",
        "x.eyJzdWIi",
        "x.eyJleHAiOjQxMDI0NDQ4MDB9.y\nr\n0",
        "header.not-base64.signature\nr",
        "a.b.c.d\nr",
    ] {
        let directory = Directory::new();
        fs::write(directory.path.join("sessao.txt"), text).unwrap();
        assert!(directory.store().load().unwrap().session.is_none());
        assert!(!directory.path.join("sessao.txt").exists());
    }
}

#[test]
fn anonymous_session_does_not_restore_a_family_profile() {
    let directory = Directory::new();
    fs::write(
        directory.path.join("sessao.txt"),
        format!("{}\nr\n1\n", token("anonymous", None, "sig")),
    )
    .unwrap();
    fs::write(directory.path.join("perfil.txt"), "3").unwrap();
    let data = directory.store().load().unwrap();
    assert!(data.session.unwrap().anonymous);
    assert!(data.selection.selected.is_none());
    assert!(data.selection.profiles.is_empty());
}

#[test]
fn migrate_profile_fields_locks_and_choice_without_confirming_this_launch() {
    let directory = Directory::new();
    fs::write(
        directory.path.join("sessao.txt"),
        format!("{}\nr\n0\n", token("user", None, "sig")),
    )
    .unwrap();
    fs::write(directory.path.join("perfil.txt"), "2\n").unwrap();
    fs::write(directory.path.join("perfis.txt"), "1\t0\t1\t0\t#1E88E5\tPrimary\thttps://image.invalid/avatar\thttps://image.invalid/background\n2\t0\t0\t1\t#AA0000\tSecond\t\t\n3\t1\t0\t0\t#000000\tKids\t\t\n").unwrap();
    let store = directory.store();
    let data = store.load().unwrap();
    assert_eq!(data.selection.profiles.len(), 3);
    assert_eq!(data.selection.suggested_slot(), 1);
    assert!(data.selection.needs_selection());
    assert!(data.selection.can_dismiss());
    assert_eq!(data.selection.addon_profile(false).unwrap().get(), 1);
    assert_eq!(data.selection.profiles[2].lock, ProfileLock::Locked);
    assert!(data.owner.is_none());
    let loaded = store.load().unwrap();
    assert_eq!(loaded.selection.profiles, data.selection.profiles);
    assert_eq!(
        loaded.selection.profiles[0].background,
        "https://image.invalid/background"
    );
    assert!(!loaded.selection.confirmed);
}

#[test]
fn logout_tombstone_prevents_reimport_and_clears_profiles() {
    let directory = Directory::new();
    let store = directory.store();
    let data = AccountData {
        session: Some(session("user", None, "a")),
        ..AccountData::default()
    };
    store.save(&data).unwrap();
    fs::write(
        directory.path.join("sessao.txt"),
        format!("{}\nr\n0", token("old-user", None, "old")),
    )
    .unwrap();
    store.clear().unwrap();
    fs::write(
        directory.path.join("sessao.txt"),
        format!("{}\nr\n0", token("old-user", None, "old")),
    )
    .unwrap();
    assert!(store.load().unwrap().session.is_none());
    assert!(store.load().unwrap().selection.profiles.is_empty());
}

#[test]
fn production_identity_and_paths_cannot_be_migrated() {
    let directory = Directory::new();
    let production = directory.parent.join(".space.mkv.nuvio");
    fs::create_dir(&production).unwrap();
    fs::write(production.join("sessao.txt"), "untouched").unwrap();
    assert!(matches!(
        AccountStore::open(&directory.path, "space.mkv.nuvio"),
        Err(Error::ProductionData)
    ));
    assert!(matches!(
        AccountStore::open(&production, TEST_APP_ID),
        Err(Error::ProductionData)
    ));
    assert_eq!(
        fs::read_to_string(production.join("sessao.txt")).unwrap(),
        "untouched"
    );
    #[cfg(unix)]
    {
        let link = directory.parent.join("nested").join(TEST_APP_ID);
        fs::create_dir(link.parent().unwrap()).unwrap();
        std::os::unix::fs::symlink(&production, &link).unwrap();
        assert!(matches!(
            AccountStore::open(&link, TEST_APP_ID),
            Err(Error::ProductionData)
        ));
    }
}

#[test]
fn future_wrong_identity_and_corrupt_json_never_fall_back_to_legacy_tokens() {
    for text in ["{", "{\"version\":2,\"app_id\":\"space.mkv.nuvio.rusttest\",\"account\":{\"session\":null,\"owner\":null,\"selection\":{\"profiles\":[],\"selected\":null}}}", "{\"version\":1,\"app_id\":\"space.mkv.nuvio\",\"account\":{\"session\":null,\"owner\":null,\"selection\":{\"profiles\":[],\"selected\":null}}}"] {
        let directory = Directory::new();
        fs::write(directory.path.join("account.json"), text).unwrap();
        fs::write(directory.path.join("sessao.txt"), format!("{}\nr\n0", token("old", None, "sig"))).unwrap();
        assert!(directory.store().load().is_err());
        assert_eq!(fs::read_to_string(directory.path.join("account.json")).unwrap(), text);
    }
}

#[test]
fn failed_migration_keeps_original_data_and_writes_are_private_and_atomic() {
    let directory = Directory::new();
    let legacy = format!("{}\nr\n0", token("user", None, "sig"));
    fs::write(directory.path.join("sessao.txt"), &legacy).unwrap();
    fs::create_dir(directory.path.join("account.json")).unwrap();
    assert!(directory.store().load().is_err());
    assert_eq!(
        fs::read_to_string(directory.path.join("sessao.txt")).unwrap(),
        legacy
    );
    fs::remove_dir(directory.path.join("account.json")).unwrap();
    directory.store().load().unwrap();
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(directory.path.join("account.json"))
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o600
        );
    }
    assert!(!fs::read_dir(&directory.path).unwrap().any(|entry| entry
        .unwrap()
        .file_name()
        .to_string_lossy()
        .ends_with(".tmp")));
}

#[test]
fn jwt_expiration_margin_missing_claims_and_redacted_debug() {
    assert!(session("user", Some(1030), "sig").expires_soon(1000));
    assert!(!session("user", Some(1031), "sig").expires_soon(1000));
    assert!(!session("user", None, "sig").expires_soon(u64::MAX));
    assert!(
        Session::from_tokens(Secret::new(token("", None, "sig")), Secret::new("r"), false).is_err()
    );
    assert!(!format!("{:?}", session("user", None, "sig")).contains("refresh-sig"));
    assert!(ProfileId::new(0).is_none());
}
