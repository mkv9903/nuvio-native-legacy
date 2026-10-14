mod support;
use nuvio_backend::{
    catalog_order::{HomeOrder, HomeOrderStorage, HomeOrderStore},
    content_context::{AddonScope, ContentContext, ContentScope},
    error::{Error, Result},
    home_order_backend::{HomeOrderAccount, HomeOrderBackend},
    home_order_controller::HomeOrderController,
    model::ProfileId,
    storage::AccountData,
};
use serde_json::{json, Value};
use std::{
    fs,
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Arc, Barrier, Mutex,
    },
    thread,
    time::{Duration, Instant},
};
use support::*;

fn context(id: u32, generation: u64) -> ContentContext {
    ContentContext {
        account_generation: 1,
        content_generation: generation,
        profile: ProfileId::new(id).unwrap(),
        force_primary: false,
        addons: AddonScope {
            member: "member".into(),
            owner: "owner".into(),
            profile: ProfileId::new(1).unwrap(),
        },
    }
}
fn order(key: &str) -> HomeOrder {
    HomeOrder::decode(&serde_json::to_vec(&json!({"order":[key]})).unwrap())
        .unwrap()
        .unwrap()
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
    fn block(&self) {
        self.started.wait();
        self.release.wait();
    }
}
struct Account {
    current: Mutex<Option<ContentContext>>,
    response: Mutex<Result<Option<HomeOrder>>>,
    calls: Mutex<Vec<ContentContext>>,
    gate: Mutex<Option<Arc<Gate>>>,
}
impl Account {
    fn select(&self, context: Option<ContentContext>) {
        *self.current.lock().unwrap() = context;
    }
    fn reply(&self, response: Result<Option<HomeOrder>>) {
        *self.response.lock().unwrap() = response;
    }
}
impl HomeOrderAccount for Account {
    fn current_context(&self) -> Option<ContentContext> {
        self.current.lock().unwrap().clone()
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.current_context().as_ref() == Some(context)
    }
    fn read_order(&self, context: &ContentContext) -> Result<Option<HomeOrder>> {
        self.calls.lock().unwrap().push(context.clone());
        let response = self.response.lock().unwrap().clone();
        let gate = self.gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.block();
        }
        response
    }
}
struct Storage {
    files: HomeOrderStore,
    load_gate: Mutex<Option<Arc<Gate>>>,
    fail_save: AtomicBool,
    clears: AtomicUsize,
}
impl HomeOrderStorage for Storage {
    fn load(&self, scope: &ContentScope) -> Result<Option<HomeOrder>> {
        let result = self.files.load(scope);
        let gate = self.load_gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.block();
        }
        result
    }
    fn save(
        &self,
        scope: &ContentScope,
        order: &HomeOrder,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        if self.fail_save.load(Ordering::SeqCst) {
            return Err(Error::Storage);
        }
        self.files.save(scope, order, current)
    }
    fn clear(&self) -> Result<()> {
        self.files.clear()?;
        self.clears.fetch_add(1, Ordering::SeqCst);
        Ok(())
    }
}
fn setup(directory: &Directory) -> (HomeOrderBackend, Arc<Account>, Arc<Storage>, ContentContext) {
    let ctx = context(2, 1);
    let account = Arc::new(Account {
        current: Mutex::new(Some(ctx.clone())),
        response: Mutex::new(Ok(Some(order("remote")))),
        calls: Mutex::new(Vec::new()),
        gate: Mutex::new(None),
    });
    let storage = Arc::new(Storage {
        files: HomeOrderStore::open(&directory.path).unwrap(),
        load_gate: Mutex::new(None),
        fail_save: AtomicBool::new(false),
        clears: AtomicUsize::new(0),
    });
    let backend = HomeOrderBackend::new(account.clone(), storage.clone());
    backend.activate();
    (backend, account, storage, ctx)
}
fn wait_for(mut ready: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(5);
    while !ready() {
        assert!(Instant::now() < deadline, "worker timed out");
        thread::sleep(Duration::from_millis(2));
    }
}

#[test]
fn service_uses_selected_profile_even_when_addons_are_shared_and_preserves_all_preferences() {
    let directory = Directory::new();
    directory
        .store()
        .save(&AccountData {
            session: Some(session("member", None, "old")),
            ..Default::default()
        })
        .unwrap();
    let account = backend(
        service(|request| match request.url.path() {
            "/rest/v1/rpc/get_sync_owner" => response(200, json!("owner")),
            "/rest/v1/rpc/sync_pull_profiles" => response(
                200,
                json!([
                    {"profile_index":1,"name":"Primary","is_primary":true,"pin_enabled":false},
                    {"profile_index":2,"name":"Shared","uses_primary_addons":true,"pin_enabled":false}
                ]),
            ),
            "/rest/v1/rpc/sync_pull_profile_locks" => response(200, json!([])),
            "/rest/v1/rpc/sync_pull_home_catalog_settings" => {
                let body: Value = serde_json::from_slice(&request.body).unwrap();
                assert_eq!(
                    body,
                    json!({"p_profile_id":2,"p_platform":"home_catalog_shared"})
                );
                assert_eq!(request.authorization.expose(), token("member", None, "old"));
                response(
                    200,
                    json!([{"settings_json":{"items":[
                {"addon_id":"recommendations","type":"movie","catalog_id":"top","order":0,"enabled":false},
                {"is_collection":true,"collection_id":"favorites","order":1}
            ],"hide_unreleased_content":true,"hide_catalog_underline":false}}]),
                )
            }
            _ => panic!("unexpected account operation"),
        }),
        &directory,
    );
    account.load_profiles().unwrap();
    account
        .select_profile(ProfileId::new(2).unwrap(), None)
        .unwrap();
    let ctx = account.content_context(false).unwrap();
    assert_eq!(ctx.addons.profile.get(), 1);
    let loaded = account.read_order(&ctx).unwrap().unwrap();
    assert_eq!(
        loaded.keys,
        ["recommendations_movie_top", "collection_favorites"]
    );
    assert!(loaded.hides("recommendations_movie_top", ""));
    assert_eq!(loaded.hide_unreleased, Some(true));
    assert_eq!(loaded.hide_underline, Some(false));
    account
        .select_profile(ProfileId::new(1).unwrap(), None)
        .unwrap();
    assert_eq!(account.read_order(&ctx), Err(Error::Cancelled));
}

#[test]
fn cached_order_is_published_before_network_finishes_and_survives_outages() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    storage
        .files
        .save(&(&ctx).into(), &order("cached"), &|| true)
        .unwrap();
    account.reply(Err(Error::Http(503)));
    let gate = Gate::new();
    *account.gate.lock().unwrap() = Some(gate.clone());
    thread::scope(|scope| {
        let task = scope.spawn(|| backend.synchronize(&ctx));
        gate.started.wait();
        assert_eq!(backend.snapshot().order.keys, ["cached"]);
        gate.release.wait();
        assert_eq!(task.join().unwrap(), Err(Error::Http(503)));
    });
    assert_eq!(backend.snapshot().order.keys, ["cached"]);
    assert_eq!(
        storage.files.load(&(&ctx).into()).unwrap().unwrap().keys,
        ["cached"]
    );
}

#[test]
fn profile_switch_shows_only_target_cache_or_empty_then_reuses_its_memory_cache() {
    let directory = Directory::new();
    let (backend, account, _, ctx) = setup(&directory);
    backend.synchronize(&ctx).unwrap();
    let held = backend.snapshot();
    let next = context(3, 2);
    account.select(Some(next.clone()));
    backend.activate();
    assert!(backend.snapshot().order.keys.is_empty());
    assert!(!backend.snapshot_current(&held));
    account.reply(Ok(Some(order("third"))));
    backend.synchronize(&next).unwrap();
    account.select(Some(context(2, 3)));
    backend.activate();
    assert_eq!(backend.snapshot().order.keys, ["remote"]);
    assert_eq!(held.order.keys, ["remote"]);
}

#[test]
fn changing_confirmed_owner_or_member_cannot_reuse_a_same_numbered_profiles_order() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    backend.synchronize(&ctx).unwrap();
    for owner_change in [true, false] {
        let mut next = ctx.clone();
        next.content_generation += 1;
        if owner_change {
            next.addons.owner = "other-owner".into();
        } else {
            next.addons.member = "other-member".into();
            next.account_generation += 1;
        }
        account.select(Some(next.clone()));
        backend.activate();
        assert!(backend.snapshot().order.keys.is_empty());
        assert!(storage.files.load(&(&next).into()).unwrap().is_none());
    }
}

#[test]
fn late_network_reply_after_profile_round_trip_never_publishes_or_replaces_cache() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    storage
        .files
        .save(&(&ctx).into(), &order("safe"), &|| true)
        .unwrap();
    let gate = Gate::new();
    *account.gate.lock().unwrap() = Some(gate.clone());
    thread::scope(|scope| {
        let task = scope.spawn(|| backend.synchronize(&ctx));
        gate.started.wait();
        account.select(Some(context(3, 2)));
        backend.activate();
        account.select(Some(context(2, 3)));
        backend.activate();
        gate.release.wait();
        assert_eq!(task.join().unwrap(), Err(Error::Cancelled));
    });
    assert_eq!(backend.snapshot().order.keys, ["safe"]);
    assert_eq!(
        storage.files.load(&(&ctx).into()).unwrap().unwrap().keys,
        ["safe"]
    );
}

#[test]
fn switching_during_cache_read_cancels_before_fetch_or_publication() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    storage
        .files
        .save(&(&ctx).into(), &order("old"), &|| true)
        .unwrap();
    let gate = Gate::new();
    *storage.load_gate.lock().unwrap() = Some(gate.clone());
    thread::scope(|scope| {
        let task = scope.spawn(|| backend.synchronize(&ctx));
        gate.started.wait();
        account.select(Some(context(3, 2)));
        backend.activate();
        gate.release.wait();
        assert_eq!(task.join().unwrap(), Err(Error::Cancelled));
    });
    assert!(backend.snapshot().order.keys.is_empty());
    assert!(account.calls.lock().unwrap().is_empty());
}

#[test]
fn absent_and_invalid_responses_retain_cache_but_confirmed_empty_clears_rows() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    backend.synchronize(&ctx).unwrap();
    account.reply(Ok(None));
    backend.synchronize(&ctx).unwrap();
    assert_eq!(backend.snapshot().order.keys, ["remote"]);
    account.reply(Err(Error::InvalidData));
    assert_eq!(backend.synchronize(&ctx), Err(Error::InvalidData));
    assert_eq!(backend.snapshot().order.keys, ["remote"]);
    account.reply(Ok(Some(HomeOrder::default())));
    backend.synchronize(&ctx).unwrap();
    assert!(backend.snapshot().order.keys.is_empty());
    assert_eq!(
        storage.files.load(&(&ctx).into()).unwrap(),
        Some(HomeOrder::default())
    );
}

#[test]
fn omitted_preferences_are_resolved_per_profile_and_saved_for_restart() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    let mut first = order("first");
    first.hide_unreleased = Some(true);
    first.hide_underline = Some(false);
    account.reply(Ok(Some(first)));
    backend.synchronize(&ctx).unwrap();
    account.reply(Ok(Some(order("next"))));
    backend.synchronize(&ctx).unwrap();
    let saved = storage.files.load(&(&ctx).into()).unwrap().unwrap();
    assert_eq!(saved.keys, ["next"]);
    assert_eq!(saved.hide_unreleased, Some(true));
    assert_eq!(saved.hide_underline, Some(false));
    let restarted = HomeOrderBackend::new(account.clone(), storage.clone());
    restarted.activate();
    account.reply(Err(Error::Network));
    assert_eq!(restarted.synchronize(&ctx), Err(Error::Network));
    assert_eq!(*restarted.snapshot().order, saved);
    let next = context(3, 2);
    account.select(Some(next.clone()));
    backend.activate();
    account.reply(Ok(Some(order("third"))));
    backend.synchronize(&next).unwrap();
    assert_eq!(backend.snapshot().order.hide_unreleased, None);
    assert_eq!(backend.snapshot().order.hide_underline, None);
}

#[test]
fn unreadable_cache_does_not_block_a_fresh_valid_reply_and_cannot_be_published() {
    let directory = Directory::new();
    let (backend, _, storage, ctx) = setup(&directory);
    fs::write(
        directory
            .path
            .join("rust-backend-v1")
            .join(HomeOrderStore::file_name(&(&ctx).into()).unwrap()),
        "damaged",
    )
    .unwrap();
    backend.synchronize(&ctx).unwrap();
    assert_eq!(backend.snapshot().order.keys, ["remote"]);
    assert!(backend.snapshot().storage_error.is_none());
    assert_eq!(
        storage.files.load(&(&ctx).into()).unwrap().unwrap().keys,
        ["remote"]
    );
}

#[test]
fn unsaved_valid_reply_is_usable_and_explicit_retry_saves_it() {
    let directory = Directory::new();
    let (backend, _, storage, ctx) = setup(&directory);
    storage.fail_save.store(true, Ordering::SeqCst);
    assert_eq!(backend.synchronize(&ctx), Err(Error::Storage));
    assert_eq!(backend.snapshot().order.keys, ["remote"]);
    assert_eq!(backend.snapshot().storage_error, Some(Error::Storage));
    storage.fail_save.store(false, Ordering::SeqCst);
    backend.synchronize(&ctx).unwrap();
    assert!(backend.snapshot().storage_error.is_none());
    assert!(storage.files.load(&(&ctx).into()).unwrap().is_some());
}

#[test]
fn ownerless_earlier_documents_are_ignored_and_save_checks_current_context() {
    let directory = Directory::new();
    let (_, _, storage, ctx) = setup(&directory);
    fs::write(
        directory.path.join("rust-backend-v1/home-order-p2.json"),
        "unproven owner",
    )
    .unwrap();
    assert!(storage.files.load(&(&ctx).into()).unwrap().is_none());
    assert_eq!(
        storage
            .files
            .save(&(&ctx).into(), &order("stale"), &|| false),
        Err(Error::Cancelled)
    );
    assert!(storage.files.load(&(&ctx).into()).unwrap().is_none());
}

#[test]
fn unverified_context_blocks_work_and_hides_previous_order() {
    let directory = Directory::new();
    let (backend, account, _, ctx) = setup(&directory);
    backend.synchronize(&ctx).unwrap();
    account.select(None);
    assert!(backend.activate().is_none());
    assert!(backend.snapshot().order.keys.is_empty());
    assert_eq!(backend.synchronize(&ctx), Err(Error::Cancelled));
    assert_eq!(account.calls.lock().unwrap().len(), 1);
}

#[test]
fn worker_queues_only_latest_context_and_keeps_frames_responsive_during_io() {
    let directory = Directory::new();
    let (backend, account, _, _) = setup(&directory);
    let controller = HomeOrderController::new(backend.clone()).unwrap();
    let gate = Gate::new();
    *account.gate.lock().unwrap() = Some(gate.clone());
    controller.step(false);
    gate.started.wait();
    account.select(Some(context(3, 2)));
    controller.step(false);
    account.select(Some(context(4, 3)));
    controller.step(false);
    assert!(backend.snapshot().order.keys.is_empty());
    gate.release.wait();
    wait_for(|| {
        backend
            .snapshot()
            .context
            .as_ref()
            .is_some_and(|ctx| ctx.profile.get() == 4)
            && backend.snapshot().order.keys == ["remote"]
    });
    assert_eq!(
        account
            .calls
            .lock()
            .unwrap()
            .iter()
            .map(|ctx| ctx.profile.get())
            .collect::<Vec<_>>(),
        [2, 4]
    );
    // Periodic work does not fire per frame, including during playback.
    for _ in 0..100 {
        controller.step(false);
    }
    assert_eq!(account.calls.lock().unwrap().len(), 2);
    backend.clear().unwrap();
}

#[test]
fn logout_clears_disk_after_inflight_work_and_hides_memory_immediately() {
    let directory = Directory::new();
    let (backend, account, storage, ctx) = setup(&directory);
    backend.synchronize(&ctx).unwrap();
    let controller = HomeOrderController::new(backend.clone()).unwrap();
    let gate = Gate::new();
    *account.gate.lock().unwrap() = Some(gate.clone());
    controller.step(false);
    gate.started.wait();
    account.select(None);
    controller.logout();
    assert!(backend.snapshot().order.keys.is_empty());
    gate.release.wait();
    wait_for(|| storage.clears.load(Ordering::SeqCst) == 1);
    assert!(storage.files.load(&(&ctx).into()).unwrap().is_none());
    assert!(backend.snapshot().context.is_none());
}
