mod support;
use nuvio_backend::addon_controller::AddonController;
use nuvio_backend::{
    addon_backend::{AddonBackend, AddonStatus},
    addon_edits::{AddonChange, AddonEdit},
    addon_service::AddonAccount,
    addon_storage::{AddonStorage, AddonStore, StoredAddons},
    addons::{AddonUrl, InstalledAddon, Manifest},
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
    model::ProfileId,
};
use std::{
    collections::HashMap,
    fs,
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Arc, Barrier, Mutex,
    },
};
use support::Directory;
use uuid::Uuid;

fn context(profile: u32, generation: u64) -> ContentContext {
    ContentContext {
        account_generation: 1,
        content_generation: generation,
        profile: ProfileId::new(profile).unwrap(),
        addons: AddonScope {
            member: "member".into(),
            owner: "owner".into(),
            profile: ProfileId::new(profile).unwrap(),
        },
        force_primary: false,
    }
}
fn addon(id: &str, enabled: bool) -> InstalledAddon {
    InstalledAddon {
        name: id.into(),
        url: AddonUrl::parse(&format!("https://{id}.invalid")).unwrap(),
        enabled,
    }
}
struct Gate {
    started: Barrier,
    release: Barrier,
}
impl Gate {
    fn new() -> Arc<Self> {
        Arc::new(Self {
            started: Barrier::new(2),
            release: Barrier::new(2),
        })
    }
    fn wait(&self) {
        self.started.wait();
        self.release.wait();
    }
}
struct Account {
    selected: Mutex<Option<ContentContext>>,
    lists: Mutex<HashMap<AddonScope, Vec<InstalledAddon>>>,
    read_error: Mutex<Option<Error>>,
    read_gate: Mutex<Option<Arc<Gate>>>,
    write_gate: Mutex<Option<Arc<Gate>>>,
    reads: AtomicUsize,
    writes: AtomicUsize,
}
impl Account {
    fn new() -> Arc<Self> {
        Arc::new(Self {
            selected: Mutex::new(Some(context(1, 1))),
            lists: Mutex::new(HashMap::new()),
            read_error: Mutex::new(None),
            read_gate: Mutex::new(None),
            write_gate: Mutex::new(None),
            reads: AtomicUsize::new(0),
            writes: AtomicUsize::new(0),
        })
    }
    fn list(&self, context: &ContentContext, list: Vec<InstalledAddon>) {
        self.lists
            .lock()
            .unwrap()
            .insert(context.addons.clone(), list);
    }
    fn select(&self, context: Option<ContentContext>) {
        *self.selected.lock().unwrap() = context;
    }
}
impl AddonAccount for Account {
    fn current_context(&self, _: bool) -> Option<ContentContext> {
        self.selected.lock().unwrap().clone()
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.selected.lock().unwrap().as_ref() == Some(context)
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        self.reads.fetch_add(1, Ordering::SeqCst);
        let result = self
            .lists
            .lock()
            .unwrap()
            .get(&context.addons)
            .cloned()
            .unwrap_or_default();
        let gate = self.read_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.wait();
        }
        if let Some(error) = self.read_error.lock().unwrap().clone() {
            return Err(error);
        }
        // Deliberately permit stale responses to test the backend's publication gate.
        Ok(result)
    }
    fn write_addons(&self, context: &ContentContext, list: &[InstalledAddon]) -> Result<()> {
        self.writes.fetch_add(1, Ordering::SeqCst);
        let gate = self.write_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.wait();
        }
        self.lists
            .lock()
            .unwrap()
            .insert(context.addons.clone(), list.to_vec());
        Ok(())
    }
}
struct Store {
    disk: AddonStore,
    fail_save: AtomicBool,
    fail_ack: AtomicBool,
}
impl Store {
    fn new(directory: &Directory) -> Arc<Self> {
        Arc::new(Self {
            disk: AddonStore::open(&directory.path).unwrap(),
            fail_save: AtomicBool::new(false),
            fail_ack: AtomicBool::new(false),
        })
    }
}
impl AddonStorage for Store {
    fn load_state(&self, scope: &AddonScope) -> Result<StoredAddons> {
        self.disk.load_state(scope)
    }
    fn load_cache(&self, scope: &AddonScope) -> Result<Option<Vec<InstalledAddon>>> {
        self.disk.load_cache(scope)
    }
    fn save_cache(&self, scope: &AddonScope, addons: &[InstalledAddon]) -> Result<()> {
        self.disk.save_cache(scope, addons)
    }
    fn load_edit(&self, scope: &AddonScope) -> Result<Option<AddonEdit>> {
        self.disk.load_edit(scope)
    }
    fn save_edit(&self, scope: &AddonScope, edit: &AddonEdit) -> Result<()> {
        self.disk.save_edit(scope, edit)
    }
    fn replace_edit(
        &self,
        scope: &AddonScope,
        expected: Option<Uuid>,
        edit: &AddonEdit,
    ) -> Result<bool> {
        if self.fail_save.load(Ordering::SeqCst) {
            Err(Error::Storage)
        } else {
            self.disk.replace_edit(scope, expected, edit)
        }
    }
    fn acknowledge(
        &self,
        scope: &AddonScope,
        id: Uuid,
        confirmed: &[InstalledAddon],
    ) -> Result<bool> {
        if self.fail_ack.load(Ordering::SeqCst) {
            Err(Error::Storage)
        } else {
            self.disk.acknowledge(scope, id, confirmed)
        }
    }
    fn clear(&self) -> Result<()> {
        self.disk.clear()
    }
}
fn setup(directory: &Directory) -> (AddonBackend, Arc<Account>, Arc<Store>, ContentContext) {
    let account = Account::new();
    let store = Store::new(directory);
    let backend = AddonBackend::new(account.clone(), store.clone());
    let context = backend.activate(false).unwrap();
    (backend, account, store, context)
}
fn displayed(backend: &AddonBackend) -> Vec<InstalledAddon> {
    backend
        .snapshot()
        .addons
        .records
        .iter()
        .map(|record| record.installed.clone())
        .collect()
}
fn payload(directory: &Directory, prefix: &str) -> std::path::PathBuf {
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
fn offline_restart_replays_intentions_then_merges_phone_additions_and_caches_uploaded_list() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    let a = addon("a", true);
    store
        .save_cache(&context.addons, &[a.clone(), addon("b", true)])
        .unwrap();
    let edit = AddonEdit::new(AddonChange::SetEnabled {
        url: a.url,
        enabled: false,
    });
    store.save_edit(&context.addons, &edit).unwrap();
    *account.read_error.lock().unwrap() = Some(Error::Network);
    assert_eq!(backend.synchronize(&context), Err(Error::Network));
    assert_eq!(displayed(&backend), [addon("a", false), addon("b", true)]);
    assert_eq!(account.writes.load(Ordering::SeqCst), 0);
    *account.read_error.lock().unwrap() = None;
    account.list(
        &context,
        vec![addon("c", true), addon("a", true), addon("b", false)],
    );
    backend.synchronize(&context).unwrap();
    let expected = vec![addon("c", true), addon("a", false), addon("b", false)];
    assert_eq!(displayed(&backend), expected);
    assert_eq!(store.load_cache(&context.addons).unwrap(), Some(expected));
    assert!(store.load_edit(&context.addons).unwrap().is_none());
    assert_eq!(backend.snapshot().status, AddonStatus::Ready);
}

#[test]
fn unreadable_pending_file_pauses_all_network_and_edits_preserves_bytes_and_can_retry() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    store
        .save_cache(&context.addons, &[addon("a", true)])
        .unwrap();
    let edit = AddonEdit::new(AddonChange::SetEnabled {
        url: addon("a", true).url,
        enabled: false,
    });
    store.save_edit(&context.addons, &edit).unwrap();
    let file = payload(&directory, "addons-edit-");
    let original = fs::read(&file).unwrap();
    fs::write(&file, "{damaged").unwrap();
    assert_eq!(
        backend.synchronize(&context),
        Err(Error::AddonJournalUnreadable)
    );
    assert_eq!(backend.snapshot().status, AddonStatus::RecoveryPaused);
    assert_eq!(displayed(&backend), [addon("a", true)]);
    assert!(backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .is_err());
    assert_eq!(account.reads.load(Ordering::SeqCst), 0);
    assert_eq!(account.writes.load(Ordering::SeqCst), 0);
    assert_eq!(fs::read_to_string(&file).unwrap(), "{damaged");
    fs::write(&file, original).unwrap();
    account.list(&context, vec![addon("a", true), addon("c", true)]);
    backend.synchronize(&context).unwrap();
    assert_eq!(displayed(&backend), [addon("a", false), addon("c", true)]);
}

#[test]
fn damaged_cache_does_not_discard_valid_pending_edits_and_fresh_fetch_repairs_cache() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    store
        .save_cache(&context.addons, &[addon("a", true)])
        .unwrap();
    store
        .save_edit(
            &context.addons,
            &AddonEdit::new(AddonChange::SetEnabled {
                url: addon("a", true).url,
                enabled: false,
            }),
        )
        .unwrap();
    fs::write(payload(&directory, "addons-cache-"), "bad cache").unwrap();
    account.list(&context, vec![addon("a", true), addon("c", true)]);
    backend.synchronize(&context).unwrap();
    assert_eq!(displayed(&backend), [addon("a", false), addon("c", true)]);
    assert!(store.load_edit(&context.addons).unwrap().is_none());
    assert!(store.load_cache(&context.addons).unwrap().is_some());
}

#[test]
fn failed_edit_save_keeps_ram_changes_across_profile_switches_and_never_uploads() {
    let directory = Directory::new();
    let (backend, account, store, first) = setup(&directory);
    account.list(&first, vec![addon("a", true)]);
    backend.synchronize(&first).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    store.fail_save.store(true, Ordering::SeqCst);
    let reads = account.reads.load(Ordering::SeqCst);
    assert_eq!(backend.synchronize(&first), Err(Error::Storage));
    assert_eq!(backend.snapshot().status, AddonStatus::Unsaved);
    assert_eq!(account.reads.load(Ordering::SeqCst), reads);
    assert_eq!(account.writes.load(Ordering::SeqCst), 0);
    let second = context(2, 2);
    account.select(Some(second.clone()));
    backend.activate(false);
    assert!(displayed(&backend).is_empty());
    account.list(&second, vec![addon("b", true)]);
    backend.synchronize(&second).unwrap();
    let returned = context(1, 3);
    account.select(Some(returned.clone()));
    backend.activate(false);
    assert_eq!(displayed(&backend), [addon("a", false)]);
    store.fail_save.store(false, Ordering::SeqCst);
    backend.synchronize(&returned).unwrap();
    assert_eq!(
        account.lists.lock().unwrap()[&first.addons],
        [addon("a", false)]
    );
}

#[test]
fn new_edit_during_upload_survives_old_acknowledgement_in_ram_and_on_disk() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true), addon("b", true)]);
    backend.synchronize(&context).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    let gate = Gate::new();
    *account.write_gate.lock().unwrap() = Some(gate.clone());
    std::thread::scope(|threads| {
        let task = threads.spawn(|| backend.synchronize(&context));
        gate.started.wait();
        let old = store.load_edit(&context.addons).unwrap().unwrap().id;
        backend
            .set_enabled(&backend.snapshot().addons, 1, false)
            .unwrap();
        gate.release.wait();
        task.join().unwrap().unwrap();
        let newer = store.load_edit(&context.addons).unwrap().unwrap();
        assert_ne!(newer.id, old);
        assert_eq!(
            newer.merge(&store.load_cache(&context.addons).unwrap().unwrap()),
            [addon("a", false), addon("b", false)]
        );
    });
    assert_eq!(displayed(&backend), [addon("a", false), addon("b", false)]);
    assert!(backend.snapshot().addons.pending);
    backend.synchronize(&context).unwrap();
    assert!(!backend.snapshot().addons.pending);
    assert_eq!(
        account.lists.lock().unwrap()[&context.addons],
        displayed(&backend)
    );
}

#[test]
fn late_read_after_profile_round_trip_cannot_publish_or_replace_target_cache() {
    let directory = Directory::new();
    let (backend, account, store, first) = setup(&directory);
    account.list(&first, vec![addon("old", true)]);
    let gate = Gate::new();
    *account.read_gate.lock().unwrap() = Some(gate.clone());
    std::thread::scope(|threads| {
        let task = threads.spawn(|| backend.synchronize(&first));
        gate.started.wait();
        account.select(Some(context(2, 2)));
        backend.activate(false);
        account.select(Some(context(1, 3)));
        backend.activate(false);
        gate.release.wait();
        assert_eq!(task.join().unwrap(), Err(Error::Cancelled));
    });
    assert!(displayed(&backend).is_empty());
    assert!(store.load_cache(&first.addons).unwrap().is_none());
}

#[test]
fn late_upload_after_switch_keeps_journal_and_does_not_publish_into_new_profile() {
    let directory = Directory::new();
    let (backend, account, store, first) = setup(&directory);
    account.list(&first, vec![addon("a", true)]);
    backend.synchronize(&first).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    let gate = Gate::new();
    *account.write_gate.lock().unwrap() = Some(gate.clone());
    std::thread::scope(|threads| {
        let task = threads.spawn(|| backend.synchronize(&first));
        gate.started.wait();
        account.select(Some(context(2, 2)));
        backend.activate(false);
        gate.release.wait();
        assert_eq!(task.join().unwrap(), Err(Error::Cancelled));
    });
    assert!(displayed(&backend).is_empty());
    assert!(store.load_edit(&first.addons).unwrap().is_some());
    assert_eq!(
        backend
            .snapshot()
            .addons
            .context
            .as_ref()
            .unwrap()
            .profile
            .get(),
        2
    );
}

#[test]
fn confirmed_empty_list_removes_cached_addons_and_toggles_cannot_resurrect_them() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    account.list(&context, Vec::new());
    backend.synchronize(&context).unwrap();
    assert!(displayed(&backend).is_empty());
    assert!(backend.snapshot().addons.authoritative);
    assert_eq!(store.load_cache(&context.addons).unwrap(), Some(Vec::new()));
    assert!(store.load_edit(&context.addons).unwrap().is_none());
}

#[test]
fn failed_acknowledgement_retains_idempotent_journal_and_retry_preserves_new_phone_addons() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    store.fail_ack.store(true, Ordering::SeqCst);
    assert_eq!(backend.synchronize(&context), Err(Error::Storage));
    assert!(store.load_edit(&context.addons).unwrap().is_some());
    account.list(&context, vec![addon("a", false), addon("c", true)]);
    store.fail_ack.store(false, Ordering::SeqCst);
    backend.synchronize(&context).unwrap();
    assert_eq!(displayed(&backend), [addon("a", false), addon("c", true)]);
    assert!(store.load_edit(&context.addons).unwrap().is_none());
}

#[test]
fn a_journal_damaged_after_startup_blocks_overwrite_and_retains_new_ram_action_on_repair() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true), addon("b", true)]);
    backend.synchronize(&context).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    *account.read_error.lock().unwrap() = Some(Error::Network);
    assert_eq!(backend.synchronize(&context), Err(Error::Network));
    let file = payload(&directory, "addons-edit-");
    let original = fs::read(&file).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 1, false)
        .unwrap();
    fs::write(&file, "{").unwrap();
    assert_eq!(
        backend.synchronize(&context),
        Err(Error::AddonJournalUnreadable)
    );
    assert_eq!(backend.snapshot().status, AddonStatus::RecoveryPaused);
    assert_eq!(fs::read_to_string(&file).unwrap(), "{");
    fs::write(&file, original).unwrap();
    *account.read_error.lock().unwrap() = None;
    backend.synchronize(&context).unwrap();
    assert_eq!(displayed(&backend), [addon("a", false), addon("b", false)]);
    assert!(store.load_edit(&context.addons).unwrap().is_none());
}

#[test]
fn another_process_edit_pauses_instead_of_overwriting_its_file_or_uploading() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    let external = AddonEdit::new(AddonChange::Install {
        addon: addon("external", true),
    });
    store.save_edit(&context.addons, &external).unwrap();
    assert_eq!(backend.synchronize(&context), Err(Error::AddonEditsChanged));
    assert_eq!(store.load_edit(&context.addons).unwrap(), Some(external));
    assert_eq!(account.writes.load(Ordering::SeqCst), 0);
    assert_eq!(backend.snapshot().status, AddonStatus::RecoveryPaused);
}

#[test]
fn manifests_have_separate_revisions_preserve_metadata_and_do_not_invalidate_ui_actions() {
    let directory = Directory::new();
    let (backend, account, _, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    let before = backend.snapshot();
    let manifest =
        Manifest::decode(br#"{"id":"example","name":"Manifest name","resources":["stream"]}"#)
            .unwrap();
    backend
        .attach_manifest(&context, &addon("a", true).url, manifest)
        .unwrap();
    let after = backend.snapshot();
    assert_eq!(before.addons.list_revision, after.addons.list_revision);
    assert_ne!(before.addons.revision, after.addons.revision);
    backend.set_enabled(&before.addons, 0, false).unwrap();
    assert_eq!(
        backend.set_enabled(&before.addons, 0, true),
        Err(Error::Cancelled)
    );
    backend.synchronize(&context).unwrap();
    assert_eq!(
        backend.snapshot().addons.records[0]
            .manifest
            .as_ref()
            .unwrap()
            .id,
        "example"
    );
}

#[test]
fn unknown_context_blocks_actions_and_logout_clears_only_rust_addon_state() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    let old = backend.snapshot();
    account.select(None);
    backend.activate(false);
    assert!(displayed(&backend).is_empty());
    assert_eq!(
        backend.set_enabled(&old.addons, 0, false),
        Err(Error::Cancelled)
    );
    fs::write(directory.path.join("addons.txt"), "legacy file").unwrap();
    backend.clear().unwrap();
    assert!(store.load_cache(&context.addons).unwrap().is_none());
    assert!(directory.path.join("addons.txt").exists());
}

#[test]
fn switching_before_the_worker_starts_still_saves_the_previous_profiles_action() {
    let directory = Directory::new();
    let (backend, account, store, first) = setup(&directory);
    account.list(&first, vec![addon("a", true)]);
    backend.synchronize(&first).unwrap();
    backend
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    account.select(Some(context(2, 2)));
    backend.activate(false);
    assert_eq!(backend.synchronize(&first), Err(Error::Cancelled));
    let saved = store.load_edit(&first.addons).unwrap().unwrap();
    assert_eq!(saved.merge(&[addon("a", true)]), [addon("a", false)]);
    assert!(displayed(&backend).is_empty());
    assert_eq!(account.writes.load(Ordering::SeqCst), 0);
}

fn wait_for(condition: impl Fn() -> bool) {
    let deadline = std::time::Instant::now() + std::time::Duration::from_secs(5);
    while !condition() {
        assert!(
            std::time::Instant::now() < deadline,
            "addon worker did not finish"
        );
        std::thread::sleep(std::time::Duration::from_millis(2));
    }
}

#[test]
fn bounded_controller_queues_only_the_latest_profile_and_hides_old_content_immediately() {
    let directory = Directory::new();
    let (backend, account, store, first) = setup(&directory);
    account.list(&first, vec![addon("old", true)]);
    let gate = Gate::new();
    *account.read_gate.lock().unwrap() = Some(gate.clone());
    let controller = AddonController::new(backend.clone()).unwrap();
    controller.step(false);
    gate.started.wait();
    let middle = context(2, 2);
    account.select(Some(middle));
    controller.step(false);
    let final_profile = context(3, 3);
    account.list(&final_profile, vec![addon("target", true)]);
    account.select(Some(final_profile.clone()));
    controller.step(false);
    assert!(displayed(&backend).is_empty());
    gate.release.wait();
    wait_for(|| backend.snapshot().status == AddonStatus::Ready);
    assert_eq!(displayed(&backend), [addon("target", true)]);
    assert_eq!(account.reads.load(Ordering::SeqCst), 2);
    assert!(store.load_cache(&first.addons).unwrap().is_none());
    assert_eq!(
        backend.snapshot().addons.context.as_ref(),
        Some(&final_profile)
    );
}

#[test]
fn controller_actions_return_during_network_work_and_new_changes_are_processed() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true), addon("b", true)]);
    backend.synchronize(&context).unwrap();
    let controller = AddonController::new(backend.clone()).unwrap();
    let gate = Gate::new();
    *account.read_gate.lock().unwrap() = Some(gate.clone());
    controller.step(false);
    gate.started.wait();
    controller
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    controller
        .set_enabled(&backend.snapshot().addons, 1, false)
        .unwrap();
    assert_eq!(displayed(&backend), [addon("a", false), addon("b", false)]);
    gate.release.wait();
    wait_for(|| backend.snapshot().status == AddonStatus::Ready);
    assert_eq!(
        account.lists.lock().unwrap()[&context.addons],
        displayed(&backend)
    );
    assert!(store.load_edit(&context.addons).unwrap().is_none());
}

#[test]
fn dropping_controller_does_not_wait_for_an_inflight_request() {
    let directory = Directory::new();
    let (backend, account, _, context) = setup(&directory);
    let gate = Gate::new();
    *account.read_gate.lock().unwrap() = Some(gate.clone());
    let controller = AddonController::new(backend.clone()).unwrap();
    controller.step(false);
    gate.started.wait();
    let (sent, received) = std::sync::mpsc::channel();
    let dropper = std::thread::spawn(move || {
        drop(controller);
        sent.send(()).unwrap();
    });
    let dropped = received.recv_timeout(std::time::Duration::from_secs(1));
    gate.release.wait();
    dropper.join().unwrap();
    assert!(dropped.is_ok(), "controller drop blocked on the network");
    wait_for(|| backend.snapshot().status == AddonStatus::Ready);
    // Wait until all operation-locked worker work ends before removing test data.
    backend.flush_edits().unwrap();
    assert_eq!(backend.snapshot().addons.context.as_ref(), Some(&context));
}

#[test]
fn logout_during_upload_hides_content_and_cleans_journal_after_the_old_request_finishes() {
    let directory = Directory::new();
    let (backend, account, store, context) = setup(&directory);
    account.list(&context, vec![addon("a", true)]);
    backend.synchronize(&context).unwrap();
    let controller = AddonController::new(backend.clone()).unwrap();
    controller.step(false);
    wait_for(|| backend.snapshot().status == AddonStatus::Ready);
    let gate = Gate::new();
    *account.write_gate.lock().unwrap() = Some(gate.clone());
    controller
        .set_enabled(&backend.snapshot().addons, 0, false)
        .unwrap();
    gate.started.wait();
    assert!(store.load_edit(&context.addons).unwrap().is_some());
    account.select(None);
    controller.logout();
    assert!(displayed(&backend).is_empty());
    assert_eq!(backend.snapshot().status, AddonStatus::Unavailable);
    gate.release.wait();
    wait_for(|| {
        store.load_edit(&context.addons).unwrap().is_none()
            && store.load_cache(&context.addons).unwrap().is_none()
    });
    assert!(store.load_cache(&context.addons).unwrap().is_none());
    assert!(displayed(&backend).is_empty());
}
