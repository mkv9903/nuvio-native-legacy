mod support;
use nuvio_backend::{
    error::Error,
    model::{Profile, ProfileId, ProfileLock, ProfileSelection, Secret, Session},
    storage::{AccountData, AccountStore},
    TEST_APP_ID,
};
use serde_json::json;
use std::fs;
use support::*;

fn envelope(data: AccountData) -> serde_json::Value {
    json!({"version":1,"app_id":TEST_APP_ID,"account":data})
}

#[test]
fn starts_fresh_and_never_reads_previous_test_attempts_files() {
    let directory = Directory::new();
    fs::write(
        directory.path.join("sessao.txt"),
        format!("{}\nr\n0", token("previous", None, "sig")),
    )
    .unwrap();
    fs::write(directory.path.join("perfil.txt"), "2").unwrap();
    fs::write(
        directory.path.join("account.json"),
        serde_json::to_vec(&envelope(AccountData {
            session: Some(session("previous", None, "sig")),
            ..AccountData::default()
        }))
        .unwrap(),
    )
    .unwrap();
    fs::create_dir(directory.path.join("standalone-v1")).unwrap();
    fs::write(
        directory.path.join("standalone-v1/session.json"),
        "previous standalone session",
    )
    .unwrap();
    let store = directory.store();
    assert!(store.load().unwrap().session.is_none());
    assert!(store.load().unwrap().selection.profiles.is_empty());
    store.clear().unwrap();
    assert!(directory.path.join("sessao.txt").exists());
    assert!(directory.path.join("standalone-v1/session.json").exists());
    assert!(store.load().unwrap().session.is_none());
}

#[test]
fn rust_sessions_and_profile_fields_round_trip_without_confirming_this_launch() {
    let directory = Directory::new();
    let profiles = vec![
        Profile {
            id: ProfileId::new(1).unwrap(),
            name: "Primary".into(),
            color: "#1E88E5".into(),
            avatar: "https://image.invalid/avatar".into(),
            avatar_id: "avatar".into(),
            background: "https://image.invalid/background".into(),
            primary: true,
            lock: ProfileLock::Unlocked,
            share_primary_addons: false,
        },
        Profile {
            id: ProfileId::new(2).unwrap(),
            name: "Second".into(),
            color: "#AA0000".into(),
            avatar: String::new(),
            avatar_id: String::new(),
            background: String::new(),
            primary: false,
            lock: ProfileLock::Unlocked,
            share_primary_addons: true,
        },
        Profile {
            id: ProfileId::new(3).unwrap(),
            name: "Kids".into(),
            color: "#000000".into(),
            avatar: String::new(),
            avatar_id: String::new(),
            background: String::new(),
            primary: false,
            lock: ProfileLock::Locked,
            share_primary_addons: false,
        },
    ];
    let data = AccountData {
        session: Some(session("member", Some(4102444800), "sig")),
        owner: Some("shared-owner".into()),
        selection: ProfileSelection {
            profiles: profiles.clone(),
            selected: ProfileId::new(2),
            confirmed: true,
        },
    };
    let store = directory.store();
    store.save(&data).unwrap();
    let loaded = store.load().unwrap();
    assert_eq!(loaded.session.unwrap().subject, "member");
    assert_eq!(loaded.owner.as_deref(), Some("shared-owner"));
    assert_eq!(loaded.selection.profiles, profiles);
    assert_eq!(loaded.selection.suggested_slot(), 1);
    assert!(loaded.selection.needs_selection());
    assert!(loaded.selection.can_dismiss());
    assert_eq!(loaded.selection.addon_profile(false).unwrap().get(), 1);
}

#[test]
fn incomplete_corrupt_and_invalid_jwt_json_is_rejected() {
    let directory = Directory::new();
    let store = directory.store();
    for body in [
        "".to_owned(),
        "{".to_owned(),
        "{}".to_owned(),
        "{\"version\":1}".to_owned(),
    ] {
        fs::write(directory.account_file(), body).unwrap();
        assert!(store.load().is_err());
    }
    for access in [
        "truncated-token",
        "x.eyJzdWIi",
        "x.eyJleHAiOjQxMDI0NDQ4MDB9.y",
        "header.not-base64.signature",
        "a.b.c.d",
    ] {
        let mut value = envelope(AccountData {
            session: Some(session("user", None, "sig")),
            ..AccountData::default()
        });
        value["account"]["session"]["access"] = json!(access);
        fs::write(
            directory.account_file(),
            serde_json::to_vec(&value).unwrap(),
        )
        .unwrap();
        assert!(store.load().is_err());
    }
}

#[test]
fn future_versions_and_wrong_app_id_are_rejected_without_rewriting_data() {
    let directory = Directory::new();
    let store = directory.store();
    for (version, id) in [(2, TEST_APP_ID), (1, "space.mkv.nuvio.native")] {
        let mut value = envelope(AccountData::default());
        value["version"] = json!(version);
        value["app_id"] = json!(id);
        let bytes = serde_json::to_vec(&value).unwrap();
        fs::write(directory.account_file(), &bytes).unwrap();
        assert!(store.load().is_err());
        assert_eq!(fs::read(directory.account_file()).unwrap(), bytes);
    }
}

#[test]
fn production_identity_paths_and_symlinks_cannot_be_read() {
    let directory = Directory::new();
    let production = directory.parent.join(".space.mkv.nuvio.native");
    fs::create_dir(&production).unwrap();
    assert!(matches!(
        AccountStore::open(&directory.path, "space.mkv.nuvio.native"),
        Err(Error::ProductionData)
    ));
    assert!(matches!(
        AccountStore::open(&production, TEST_APP_ID),
        Err(Error::ProductionData)
    ));
    #[cfg(unix)]
    {
        let path = directory.parent.join("linked").join(TEST_APP_ID);
        fs::create_dir(path.parent().unwrap()).unwrap();
        std::os::unix::fs::symlink(&production, &path).unwrap();
        assert!(matches!(
            AccountStore::open(path, TEST_APP_ID),
            Err(Error::ProductionData)
        ));
        let store = directory.store();
        let production_file = production.join("account.json");
        fs::write(&production_file, b"untouched").unwrap();
        std::os::unix::fs::symlink(&production_file, directory.account_file()).unwrap();
        assert!(store.load().is_err());
        assert_eq!(fs::read(&production_file).unwrap(), b"untouched");
    }
}

#[test]
fn writes_are_private_atomic_and_logout_clears_current_rust_account() {
    let directory = Directory::new();
    let store = directory.store();
    store
        .save(&AccountData {
            session: Some(session("user", None, "sig")),
            ..AccountData::default()
        })
        .unwrap();
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(directory.account_file())
                .unwrap()
                .permissions()
                .mode()
                & 0o777,
            0o600
        );
    }
    assert!(!fs::read_dir(directory.account_file().parent().unwrap())
        .unwrap()
        .any(|entry| entry
            .unwrap()
            .file_name()
            .to_string_lossy()
            .ends_with(".tmp")));
    store.clear().unwrap();
    assert!(store.load().unwrap().session.is_none());
    assert!(store.load().unwrap().selection.profiles.is_empty());
}

#[test]
fn oversized_storage_and_tampered_subject_or_expiration_are_rejected() {
    let directory = Directory::new();
    let store = directory.store();
    fs::write(directory.account_file(), vec![b'a'; 1024 * 1024 + 1]).unwrap();
    assert!(matches!(store.load(), Err(Error::ResponseLimit)));
    for field in ["subject", "expires_at"] {
        let mut value = envelope(AccountData {
            session: Some(session("user", Some(5000), "sig")),
            ..AccountData::default()
        });
        value["account"]["session"][field] = if field == "subject" {
            json!("another-user")
        } else {
            json!(6000)
        };
        fs::write(
            directory.account_file(),
            serde_json::to_vec(&value).unwrap(),
        )
        .unwrap();
        assert!(matches!(store.load(), Err(Error::InvalidSession)));
    }
}

#[test]
fn expiration_margin_missing_claims_and_redacted_debug() {
    assert!(session("user", Some(1030), "sig").expires_soon(1000));
    assert!(!session("user", Some(1031), "sig").expires_soon(1000));
    assert!(!session("user", None, "sig").expires_soon(u64::MAX));
    assert!(
        Session::from_tokens(Secret::new(token("", None, "sig")), Secret::new("r"), false).is_err()
    );
    assert!(!format!("{:?}", session("user", None, "sig")).contains("refresh-sig"));
    assert!(ProfileId::new(0).is_none());
}
