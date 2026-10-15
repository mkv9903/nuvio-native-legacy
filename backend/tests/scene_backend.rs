mod support;
use nuvio_backend::{
    addon_backend::{AddonBackendSnapshot, AddonStatus},
    addon_snapshot::{AddonRecord, AddonSnapshot},
    addons::{InstalledAddon, Manifest},
    catalog_scene::{CatalogRowState, CatalogScene},
    content_context::{AddonScope, ContentContext, ContentScope},
    error::{Error, Result},
    model::ProfileId,
    scene_backend::{SceneBackend, SceneSource, SceneStorage},
    scene_cache::SceneCache,
    scene_snapshot::SceneSnapshot,
};
use std::{
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Arc, Barrier, Mutex,
    },
    thread,
};
use support::{scene::complete_scene, Directory};

fn context(profile: u32, generation: u64) -> ContentContext {
    ContentContext {
        account_generation: 1,
        content_generation: generation,
        profile: ProfileId::new(profile).unwrap(),
        force_primary: false,
        addons: AddonScope {
            member: "member".into(),
            owner: "owner".into(),
            profile: ProfileId::new(1).unwrap(),
        },
    }
}
fn source(profile: u32, generation: u64, list_revision: u64) -> Arc<AddonBackendSnapshot> {
    Arc::new(AddonBackendSnapshot {
        addons: AddonSnapshot {
            context: Some(context(profile, generation)),
            revision: list_revision,
            list_revision,
            records: Arc::from([AddonRecord {
                installed: InstalledAddon {
                    name: "Provider".into(),
                    url: complete_scene().rows[0]
                        .source
                        .as_ref()
                        .unwrap()
                        .addon
                        .clone(),
                    enabled: true,
                },
                manifest: None,
                silent_queries: 0,
            }]),
            authoritative: true,
            pending: false,
        },
        status: AddonStatus::Ready,
        error: None,
    })
}
struct Environment(Mutex<Arc<AddonBackendSnapshot>>);
impl Environment {
    fn select(&self, source: Arc<AddonBackendSnapshot>) {
        *self.0.lock().unwrap() = source;
    }
}
impl SceneSource for Environment {
    fn snapshot(&self) -> Arc<AddonBackendSnapshot> {
        self.0.lock().unwrap().clone()
    }
    fn current(&self, source: &AddonSnapshot) -> bool {
        let current = self.snapshot();
        current.addons.context == source.context
            && current.addons.list_revision == source.list_revision
            && current
                .addons
                .records
                .iter()
                .map(|record| &record.installed)
                .eq(source.records.iter().map(|record| &record.installed))
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
struct Storage {
    files: SceneCache,
    load_gate: Mutex<Option<Arc<Gate>>>,
    save_gate: Mutex<Option<Arc<Gate>>>,
    clear_gate: Mutex<Option<Arc<Gate>>>,
    fail_save: AtomicBool,
    loads: AtomicUsize,
}
impl SceneStorage for Storage {
    fn load(
        &self,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Option<Arc<SceneSnapshot>>> {
        self.loads.fetch_add(1, Ordering::SeqCst);
        let scene = <SceneCache as SceneStorage>::load(&self.files, scope, current);
        let gate = self.load_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.wait();
        }
        scene
    }
    fn save(
        &self,
        scope: &ContentScope,
        scene: &SceneSnapshot,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        let gate = self.save_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.wait();
        }
        if self.fail_save.load(Ordering::SeqCst) {
            return Err(Error::Storage);
        }
        <SceneCache as SceneStorage>::save(&self.files, scope, scene, current)
    }
    fn clear(&self) -> Result<()> {
        let gate = self.clear_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.wait();
        }
        self.files.clear()
    }
}
fn setup(directory: &Directory) -> (SceneBackend, Arc<Environment>, Arc<Storage>) {
    let environment = Arc::new(Environment(Mutex::new(source(2, 1, 1))));
    let storage = Arc::new(Storage {
        files: SceneCache::open(&directory.path).unwrap(),
        load_gate: Mutex::new(None),
        save_gate: Mutex::new(None),
        clear_gate: Mutex::new(None),
        fail_save: AtomicBool::new(false),
        loads: AtomicUsize::new(0),
    });
    let backend = SceneBackend::new(environment.clone(), storage.clone());
    backend.activate().unwrap();
    (backend, environment, storage)
}
fn scene(id: &str) -> CatalogScene {
    let mut scene = complete_scene();
    scene.titles[0].metadata.id = id.into();
    scene
}
fn id(backend: &SceneBackend) -> Option<String> {
    backend
        .snapshot()
        .scene
        .as_ref()
        .and_then(|scene| scene.titles.first())
        .map(|title| title.record.metadata.id.clone())
}
fn scope(profile: u32) -> ContentScope {
    ContentScope::from(&context(profile, 1))
}

#[test]
fn profile_switch_publishes_only_the_target_cache_even_when_addons_are_shared() {
    let directory = Directory::new();
    let (backend, environment, storage) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("profile-2"))
        .unwrap();
    let old_view = backend.snapshot();
    storage
        .files
        .save(&scope(3), &scene("profile-3"), &|| true)
        .unwrap();
    environment.select(source(3, 2, 2));
    assert!(!backend.snapshot_current(&old_view));
    backend.activate().unwrap();
    assert!(id(&backend).is_none());
    backend.restore(&backend.begin_restore().unwrap()).unwrap();
    assert_eq!(id(&backend).as_deref(), Some("profile-3"));
    assert!(backend.snapshot().from_cache);
    environment.select(source(2, 3, 3));
    backend.activate().unwrap();
    assert_eq!(id(&backend).as_deref(), Some("profile-2"));
}

#[test]
fn source_changes_and_profile_round_trips_cancel_old_work_before_any_publication_or_save() {
    let directory = Directory::new();
    let (backend, environment, storage) = setup(&directory);
    let old = backend.begin_publish().unwrap();
    environment.select(source(3, 2, 2));
    backend.activate().unwrap();
    environment.select(source(2, 3, 3));
    backend.activate().unwrap();
    assert_eq!(
        backend.publish(&old, scene("old-response")),
        Err(Error::Cancelled)
    );
    assert!(storage.files.load(&scope(2), &|| true).unwrap().is_none());
    let old = backend.begin_publish().unwrap();
    let mut configured = (*source(2, 3, 4)).clone();
    let mut records = configured.addons.records.to_vec();
    records[0].installed.enabled = false;
    configured.addons.records = records.into();
    environment.select(Arc::new(configured));
    backend.activate().unwrap();
    assert!(!backend.current(&old));
}

#[test]
fn delayed_disk_restore_cannot_replace_a_new_network_scene() {
    let directory = Directory::new();
    let (backend, _, storage) = setup(&directory);
    storage
        .files
        .save(&scope(2), &scene("disk"), &|| true)
        .unwrap();
    let gate = Gate::new();
    *storage.load_gate.lock().unwrap() = Some(gate.clone());
    let work = backend.begin_restore().unwrap();
    let worker = backend.clone();
    let thread = thread::spawn(move || worker.restore(&work));
    gate.started.wait();
    backend
        .publish(&backend.begin_publish().unwrap(), scene("network"))
        .unwrap();
    gate.release.wait();
    assert_eq!(thread.join().unwrap(), Err(Error::Cancelled));
    assert_eq!(id(&backend).as_deref(), Some("network"));
}

#[test]
fn newer_requests_and_newer_accepts_within_one_request_cancel_stale_file_writes() {
    let directory = Directory::new();
    let (backend, _, storage) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("initial"))
        .unwrap();
    let old = backend.begin_publish().unwrap();
    let next = backend.begin_publish().unwrap();
    assert_eq!(backend.publish(&old, scene("stale")), Err(Error::Cancelled));
    let first = Arc::new(SceneSnapshot::new(scene("partial")).unwrap());
    backend.accept(&next, first.clone()).unwrap();
    let gate = Gate::new();
    *storage.save_gate.lock().unwrap() = Some(gate.clone());
    let work = next.clone();
    let worker = backend.clone();
    let thread = thread::spawn(move || worker.save(&work, &first));
    gate.started.wait();
    let complete = Arc::new(SceneSnapshot::new(scene("complete")).unwrap());
    backend.accept(&next, complete.clone()).unwrap();
    gate.release.wait();
    assert_eq!(thread.join().unwrap(), Err(Error::Cancelled));
    backend.save(&next, &complete).unwrap();
    assert_eq!(
        storage
            .files
            .load(&scope(2), &|| true)
            .unwrap()
            .unwrap()
            .titles[0]
            .metadata
            .id,
        "complete"
    );
}

#[test]
fn confirmed_empty_addons_remove_their_rows_and_titles_but_keep_profile_watch_state() {
    let directory = Directory::new();
    let (backend, environment, _) = setup(&directory);
    let mut expected = scene("addon-only");
    expected.titles[0].resume.percentage = 0;
    expected.titles[0].on_watchlist = false;
    expected.titles[0].in_collection = false;
    let mut saved = expected.titles[0].clone();
    saved.metadata.id = "saved".into();
    saved.on_watchlist = true;
    expected.titles.push(saved);
    backend
        .publish(&backend.begin_publish().unwrap(), expected)
        .unwrap();
    let mut empty = (*source(2, 1, 2)).clone();
    empty.addons.records = Arc::from([]);
    environment.select(Arc::new(empty));
    backend.activate().unwrap();
    let view = backend.snapshot();
    let actual = view.scene.as_ref().unwrap();
    assert!(actual.rows.is_empty());
    assert_eq!(actual.titles.len(), 1);
    assert_eq!(actual.titles[0].record.metadata.id, "saved");
}

#[test]
fn valid_empty_manifest_declarations_hide_cached_catalogs_and_counter_changes_do_not_reset_work() {
    let directory = Directory::new();
    let (backend, environment, _) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("visible"))
        .unwrap();
    let work = backend.begin_publish().unwrap();
    let mut changed = (*environment.snapshot()).clone();
    let mut records = changed.addons.records.to_vec();
    records[0].silent_queries = 1;
    changed.addons.revision += 1;
    changed.addons.records = records.into();
    let revision = backend.snapshot().revision;
    environment.select(Arc::new(changed));
    backend.activate().unwrap();
    assert!(backend.current(&work));
    assert_eq!(backend.snapshot().revision, revision);
    let mut changed = (*environment.snapshot()).clone();
    let mut records = changed.addons.records.to_vec();
    records[0].manifest = Some(Arc::new(
        Manifest::decode(br#"{"id":"provider","resources":[],"catalogs":[]}"#).unwrap(),
    ));
    changed.addons.records = records.into();
    environment.select(Arc::new(changed));
    backend.activate().unwrap();
    assert!(backend.snapshot().scene.as_ref().unwrap().rows.is_empty());
}

#[test]
fn storage_failure_keeps_fresh_display_data_and_recovery_preserves_empty_scene_authority() {
    let directory = Directory::new();
    let (backend, _, storage) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("cached"))
        .unwrap();
    storage.fail_save.store(true, Ordering::SeqCst);
    assert_eq!(
        backend.publish(&backend.begin_publish().unwrap(), scene("fresh")),
        Err(Error::Storage)
    );
    assert_eq!(id(&backend).as_deref(), Some("fresh"));
    assert_eq!(backend.snapshot().error, Some(Error::Storage));
    assert_eq!(
        storage
            .files
            .load(&scope(2), &|| true)
            .unwrap()
            .unwrap()
            .titles[0]
            .metadata
            .id,
        "cached"
    );
    storage.fail_save.store(false, Ordering::SeqCst);
    backend
        .publish(&backend.begin_publish().unwrap(), CatalogScene::default())
        .unwrap();
    assert!(backend.snapshot().scene.as_ref().unwrap().titles.is_empty());
    assert!(storage
        .files
        .load(&scope(2), &|| true)
        .unwrap()
        .unwrap()
        .titles
        .is_empty());
}

#[test]
fn unverified_owners_and_unconfirmed_addon_lists_never_open_or_display_profile_storage() {
    let directory = Directory::new();
    let (backend, environment, storage) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("private"))
        .unwrap();
    for unknown_owner in [false, true] {
        let mut unverified = (*source(2, 1, 2)).clone();
        if unknown_owner {
            unverified
                .addons
                .context
                .as_mut()
                .unwrap()
                .addons
                .owner
                .clear();
        } else {
            unverified.addons.authoritative = false;
        }
        environment.select(Arc::new(unverified));
        backend.activate().unwrap();
        assert!(backend.snapshot().scene.is_none());
        assert!(matches!(backend.begin_restore(), Err(Error::Cancelled)));
    }
    assert_eq!(storage.loads.load(Ordering::SeqCst), 0);
}

#[test]
fn clearing_cancels_old_writes_and_blocks_restore_until_disk_cleanup_finishes() {
    let directory = Directory::new();
    let (backend, _, storage) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("cached"))
        .unwrap();
    let old = backend.begin_publish().unwrap();
    let gate = Gate::new();
    *storage.clear_gate.lock().unwrap() = Some(gate.clone());
    let worker = backend.clone();
    let thread = thread::spawn(move || worker.clear());
    gate.started.wait();
    backend.activate().unwrap();
    assert!(backend.snapshot().scene.is_none());
    assert!(matches!(backend.begin_restore(), Err(Error::Cancelled)));
    assert!(!backend.current(&old));
    gate.release.wait();
    thread.join().unwrap().unwrap();
    backend.activate().unwrap();
    backend.restore(&backend.begin_restore().unwrap()).unwrap();
    assert!(id(&backend).is_none());
}

#[test]
fn malformed_native_scenes_cannot_replace_a_valid_publication() {
    let directory = Directory::new();
    let (backend, _, _) = setup(&directory);
    backend
        .publish(&backend.begin_publish().unwrap(), scene("valid"))
        .unwrap();
    let work = backend.begin_publish().unwrap();
    let mut invalid = SceneSnapshot::new(complete_scene()).unwrap();
    invalid.titles.clear();
    assert_eq!(
        backend.accept(&work, Arc::new(invalid)),
        Err(Error::InvalidData)
    );
    assert_eq!(id(&backend).as_deref(), Some("valid"));
}

#[test]
fn shared_title_windows_are_reindexed_when_a_catalog_disappears() {
    let directory = Directory::new();
    let (backend, environment, _) = setup(&directory);
    let mut data = scene("first");
    data.titles[0].on_watchlist = false;
    data.titles[0].in_collection = false;
    data.titles[0].resume.percentage = 0;
    let mut next = data.titles[0].clone();
    next.metadata.id = "second".into();
    data.titles.push(next);
    let mut fixed = data.rows[0].clone();
    fixed.key = "continue_watching".into();
    fixed.source = None;
    fixed.titles = 1..2;
    fixed.state = CatalogRowState::Ready;
    data.rows.push(fixed);
    backend
        .publish(&backend.begin_publish().unwrap(), data)
        .unwrap();
    let mut empty = (*source(2, 1, 2)).clone();
    empty.addons.records = Arc::from([]);
    environment.select(Arc::new(empty));
    backend.activate().unwrap();
    let view = backend.snapshot();
    let actual = view.scene.as_ref().unwrap();
    assert_eq!(actual.titles.len(), 1);
    assert_eq!(actual.titles[0].record.metadata.id, "second");
    assert_eq!(actual.rows[0].record.titles, 0..1);
}
