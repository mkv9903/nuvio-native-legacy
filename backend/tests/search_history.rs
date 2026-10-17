mod support;
use nuvio_backend::{
    content_context::ContentScope,
    error::Error,
    model::ProfileId,
    search_history::{SearchHistory, SearchHistoryStore, HISTORY_ENTRIES},
};
use std::{
    fs,
    sync::atomic::{AtomicUsize, Ordering},
};
use support::Directory;
fn scope(profile: u32) -> ContentScope {
    ContentScope {
        member: "member".into(),
        owner: "owner".into(),
        profile: ProfileId::new(profile).unwrap(),
    }
}
#[test]
fn confirmed_queries_trim_deduplicate_and_move_to_the_front() {
    let mut history = SearchHistory::default();
    for short in ["", "a", "   x  ", "日", "e\u{301}"] {
        assert!(!history.record(short));
    }
    assert!(history.record("up"));
    history.record(" matrix ");
    history.record("dune");
    history.record("MATRIX ");
    assert_eq!(history.queries(), ["MATRIX", "dune", "up"]);
    history.record("\tCafé\n");
    history.record("CAFE\u{301}");
    assert_eq!(
        history
            .queries()
            .iter()
            .filter(|q| q.starts_with("Caf") || q.starts_with("CAF"))
            .count(),
        1
    );
}
#[test]
fn ten_most_recent_queries_survive_promotion_removal_and_clear() {
    let mut history = SearchHistory::default();
    for index in 0..13 {
        history.record(&format!("query {index:02}"));
    }
    assert_eq!(history.queries().len(), HISTORY_ENTRIES);
    assert_eq!(history.queries()[9], "query 03");
    history.record("query 05");
    assert_eq!(history.queries()[0], "query 05");
    assert_eq!(history.queries().len(), HISTORY_ENTRIES);
    history.remove(1);
    assert_eq!(history.queries()[1], "query 11");
    history.remove(1000);
    assert_eq!(history.queries().len(), 9);
    history.clear();
    assert!(history.queries().is_empty());
}
#[test]
fn rust_json_round_trips_full_queries_and_isolates_account_owner_and_selected_profile() {
    let directory = Directory::new();
    let store = SearchHistoryStore::open(&directory.path).unwrap();
    let mut history = SearchHistory::default();
    let full = "Complete query 日 ".repeat(400);
    history.record(&full);
    history.record("last query");
    store.save(&scope(1), &history, &|| true).unwrap();
    let reopened = SearchHistoryStore::open(&directory.path).unwrap();
    assert_eq!(reopened.load(&scope(1), &|| true).unwrap(), history);
    assert!(reopened
        .load(&scope(2), &|| true)
        .unwrap()
        .queries()
        .is_empty());
    let mut other = scope(1);
    other.member = "other member".into();
    assert!(reopened
        .load(&other, &|| true)
        .unwrap()
        .queries()
        .is_empty());
    other = scope(1);
    other.owner = "other owner".into();
    assert!(reopened
        .load(&other, &|| true)
        .unwrap()
        .queries()
        .is_empty());
}
#[test]
fn cancellation_does_not_overwrite_confirmed_history() {
    let directory = Directory::new();
    let store = SearchHistoryStore::open(&directory.path).unwrap();
    let mut history = SearchHistory::default();
    history.record("original");
    store.save(&scope(1), &history, &|| true).unwrap();
    history.record("late query");
    let calls = AtomicUsize::new(0);
    assert_eq!(
        store.save(&scope(1), &history, &|| calls
            .fetch_add(1, Ordering::SeqCst)
            < 2),
        Err(Error::Cancelled)
    );
    assert_eq!(
        store.load(&scope(1), &|| true).unwrap().queries(),
        ["original"]
    );
    assert_eq!(store.load(&scope(1), &|| false), Err(Error::Cancelled));
    assert!(fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .all(|entry| !entry
            .unwrap()
            .file_name()
            .to_string_lossy()
            .ends_with(".tmp")));
}
#[test]
fn original_files_are_ignored_and_logout_removes_only_rust_history() {
    let directory = Directory::new();
    let store = SearchHistoryStore::open(&directory.path).unwrap();
    fs::write(directory.path.join("buscas-p1.txt"), "original app history").unwrap();
    assert!(store
        .load(&scope(1), &|| true)
        .unwrap()
        .queries()
        .is_empty());
    let mut history = SearchHistory::default();
    history.record("confirmed");
    store.save(&scope(1), &history, &|| true).unwrap();
    store.save(&scope(2), &history, &|| true).unwrap();
    let account = directory.path.join("rust-backend-v1/account.json");
    fs::write(&account, "preserve").unwrap();
    store.clear().unwrap();
    assert!(store
        .load(&scope(1), &|| true)
        .unwrap()
        .queries()
        .is_empty());
    assert!(store
        .load(&scope(2), &|| true)
        .unwrap()
        .queries()
        .is_empty());
    assert_eq!(fs::read_to_string(account).unwrap(), "preserve");
    assert!(directory.path.join("buscas-p1.txt").exists());
}
#[test]
fn invalid_documents_are_rejected_and_clearing_history_removes_its_file() {
    let directory = Directory::new();
    let store = SearchHistoryStore::open(&directory.path).unwrap();
    let mut history = SearchHistory::default();
    history.record("confirmed");
    store.save(&scope(1), &history, &|| true).unwrap();
    let path = fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| path.extension().is_some_and(|ext| ext == "json"))
        .unwrap();
    let original: serde_json::Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
    for change in [
        serde_json::json!({"version":9}),
        serde_json::json!({"queries":["x"]}),
        serde_json::json!({"queries":["same","SAME"]}),
        serde_json::json!({"scope":scope(2)}),
    ] {
        let mut body = original.clone();
        for (key, value) in change.as_object().unwrap() {
            body[key] = value.clone();
        }
        fs::write(&path, serde_json::to_vec(&body).unwrap()).unwrap();
        assert_eq!(store.load(&scope(1), &|| true), Err(Error::InvalidData));
    }
    store
        .save(&scope(1), &SearchHistory::default(), &|| true)
        .unwrap();
    assert!(!path.exists());
}
