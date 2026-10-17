mod support;
use nuvio_backend::{
    content_context::ContentScope,
    error::{Error, Result},
    model::ProfileId,
    search_history::{SearchHistory, SearchHistoryStore},
    search_history_runtime::{HistoryAccount, HistoryStorage, SearchHistoryRuntime},
};
use std::{
    fs,
    sync::{Arc, Barrier, Mutex},
    thread,
    time::{Duration, Instant},
};
use support::Directory;
fn scope(id: u32) -> ContentScope {
    ContentScope {
        member: "member".into(),
        owner: "owner".into(),
        profile: ProfileId::new(id).unwrap(),
    }
}
struct Account(Mutex<Option<ContentScope>>);
impl HistoryAccount for Account {
    fn history_scope(&self) -> Option<ContentScope> {
        self.0.lock().unwrap().clone()
    }
}
fn wait(mut condition: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(5);
    while !condition() {
        assert!(Instant::now() < deadline, "history worker timed out");
        thread::sleep(Duration::from_millis(5));
    }
}
struct Gate {
    entered: Barrier,
    release: Barrier,
}
impl Gate {
    fn new() -> Arc<Self> {
        Arc::new(Self {
            entered: Barrier::new(2),
            release: Barrier::new(2),
        })
    }
    fn block(&self) {
        self.entered.wait();
        self.release.wait();
    }
}
struct Storage {
    files: SearchHistoryStore,
    load: Mutex<Option<Arc<Gate>>>,
    save: Mutex<Option<Arc<Gate>>>,
}
impl HistoryStorage for Storage {
    fn load(&self, scope: &ContentScope, current: &dyn Fn() -> bool) -> Result<SearchHistory> {
        let result = self.files.load(scope, current);
        let gate = self.load.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.block();
        }
        if !current() {
            return Err(Error::Cancelled);
        }
        result
    }
    fn save(
        &self,
        scope: &ContentScope,
        history: &SearchHistory,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        let gate = self.save.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.block();
        }
        self.files.save(scope, history, current)
    }
    fn clear(&self) -> Result<()> {
        self.files.clear()
    }
}
fn setup(directory: &Directory) -> (Arc<Account>, Arc<Storage>) {
    (
        Arc::new(Account(Mutex::new(Some(scope(1))))),
        Arc::new(Storage {
            files: SearchHistoryStore::open(&directory.path).unwrap(),
            load: Mutex::new(None),
            save: Mutex::new(None),
        }),
    )
}
#[test]
fn selection_is_immediate_and_each_profile_restores_only_its_own_rust_history() {
    let directory = Directory::new();
    let (account, storage) = setup(&directory);
    let runtime = SearchHistoryRuntime::new(account.clone(), storage.clone()).unwrap();
    assert!(runtime.record("Profile one"));
    *account.0.lock().unwrap() = Some(scope(2));
    runtime.step();
    assert!(runtime.snapshot().history.queries().is_empty());
    runtime.record("Profile two");
    *account.0.lock().unwrap() = Some(scope(1));
    runtime.step();
    assert_eq!(runtime.snapshot().history.queries(), ["Profile one"]);
    drop(runtime); // Normal shutdown drains the serial worker, including the outgoing profile.
    assert_eq!(
        storage.files.load(&scope(1), &|| true).unwrap().queries(),
        ["Profile one"]
    );
    assert_eq!(
        storage.files.load(&scope(2), &|| true).unwrap().queries(),
        ["Profile two"]
    );
    let runtime = SearchHistoryRuntime::new(account, storage).unwrap();
    runtime.step();
    wait(|| !runtime.snapshot().loading);
    assert_eq!(runtime.snapshot().history.queries(), ["Profile one"]);
}
#[test]
fn edits_during_restore_merge_in_order_and_clear_does_not_restore_deleted_terms() {
    let directory = Directory::new();
    let (account, storage) = setup(&directory);
    let mut previous = SearchHistory::default();
    previous.record("Old query");
    storage.files.save(&scope(1), &previous, &|| true).unwrap();
    let gate = Gate::new();
    *storage.load.lock().unwrap() = Some(gate.clone());
    let runtime = SearchHistoryRuntime::new(account, storage.clone()).unwrap();
    runtime.step();
    gate.entered.wait();
    runtime.record("Before clear");
    runtime.clear_current();
    runtime.record("New query");
    assert_eq!(runtime.snapshot().history.queries(), ["New query"]);
    gate.release.wait();
    wait(|| !runtime.snapshot().loading);
    drop(runtime);
    assert_eq!(
        storage.files.load(&scope(1), &|| true).unwrap().queries(),
        ["New query"]
    );
}
#[test]
fn an_edit_during_a_write_is_not_marked_saved_or_lost() {
    let directory = Directory::new();
    let (account, storage) = setup(&directory);
    let gate = Gate::new();
    *storage.save.lock().unwrap() = Some(gate.clone());
    let runtime = SearchHistoryRuntime::new(account, storage.clone()).unwrap();
    runtime.record("First query");
    gate.entered.wait();
    runtime.record("Second query");
    runtime.remove(1);
    gate.release.wait();
    drop(runtime);
    assert_eq!(
        storage.files.load(&scope(1), &|| true).unwrap().queries(),
        ["Second query"]
    );
}
#[test]
fn logout_cancels_an_old_write_before_clearing_and_never_erases_other_capabilities() {
    let directory = Directory::new();
    let (account, storage) = setup(&directory);
    fs::write(directory.path.join("other-capability.json"), "untouched").unwrap();
    let gate = Gate::new();
    *storage.save.lock().unwrap() = Some(gate.clone());
    let runtime = SearchHistoryRuntime::new(account.clone(), storage.clone()).unwrap();
    runtime.record("Private old search");
    gate.entered.wait();
    *account.0.lock().unwrap() = None;
    runtime.logout();
    assert!(runtime.snapshot().history.queries().is_empty());
    gate.release.wait();
    drop(runtime);
    assert!(storage
        .files
        .load(&scope(1), &|| true)
        .unwrap()
        .queries()
        .is_empty());
    assert_eq!(
        fs::read_to_string(directory.path.join("other-capability.json")).unwrap(),
        "untouched"
    );
}
#[test]
fn account_identity_is_part_of_scope_and_unreadable_history_is_protected() {
    let directory = Directory::new();
    let (account, storage) = setup(&directory);
    let runtime = SearchHistoryRuntime::new(account.clone(), storage.clone()).unwrap();
    runtime.record("First account");
    drop(runtime);
    let path = fs::read_dir(directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| path.extension().is_some_and(|e| e == "json"))
        .unwrap();
    fs::write(&path, "damaged private Rust history").unwrap();
    let runtime = SearchHistoryRuntime::new(account.clone(), storage.clone()).unwrap();
    runtime.step();
    wait(|| runtime.snapshot().error.is_some());
    runtime.record("Not an automatic reset");
    let mut other = scope(1);
    other.member = "another member".into();
    *account.0.lock().unwrap() = Some(other.clone());
    runtime.step();
    assert!(runtime.snapshot().history.queries().is_empty());
    runtime.record("Other account");
    drop(runtime);
    assert_eq!(
        fs::read_to_string(path).unwrap(),
        "damaged private Rust history"
    );
    assert_eq!(
        storage.files.load(&other, &|| true).unwrap().queries(),
        ["Other account"]
    );
}
