#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    addon_backend::AddonBackend,
    addon_controller::AddonController,
    addon_ffi::NativeAddonController,
    addon_service::AddonAccount,
    addon_storage::AddonStore,
    addons::{InstalledAddon, Manifest},
    catalog_cache::{CatalogCache, ProviderCache, ProviderResource},
    catalog_ffi::*,
    catalog_runtime::CatalogRuntime,
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
    home_loading_ffi::*,
    http::Response,
    model::ProfileId,
    provider_http::ProviderTransport,
    scene_cache::SceneCache,
    scene_snapshot::SceneSnapshot,
    title_snapshot::TitleField,
};
use serde_json::json;
use std::{
    collections::HashMap,
    ffi::{c_char, c_void, CStr, CString},
    sync::{Arc, Condvar, Mutex},
    time::{Duration, Instant},
};
use support::Directory;
use url::Url;
use zeroize::Zeroizing;
#[link(name = "nuvio_c_ui")]
extern "C" {
    fn account_ui_test_attach(account: *mut c_void);
    fn profiles_active_addons() -> i32;
    fn data_init(art: *const c_char);
    fn fil_definir_perfil(profile: i32);
    fn fil_perfil_geracao() -> u32;
    fn fil_n() -> i32;
    fn register_profile_row(
        profile: i32,
        generation: u32,
        key: *const c_char,
        name: *const c_char,
        addon: *const c_char,
        kind: *const c_char,
        items: i32,
        suggestion: i32,
    ) -> i32;
    fn discovery_test_home_loading(
        plan: *mut NativeHomePlan,
        work: *const NativeCatalogWork,
        continuing: *const c_void,
        saved: *const c_void,
        limit: i32,
        stats: *mut NativeLoadStats,
    ) -> i32;
    fn catalog_runtime_test_attach(runtime: *const CatalogRuntime);
    fn catalog_runtime_step();
    fn catalog_runtime_cycle_begin();
    fn catalog_runtime_cycle_end();
    fn addon_registry_refresh();
    fn addons_n() -> i32;
    fn addons_base(index: i32) -> *const c_char;
    fn catalog_runtime_use(work: *const NativeCatalogWork);
    fn catalog_runtime_capture(save: i32) -> i32;
    fn catalog_runtime_mark_published();
    fn catalog_runtime_published_current() -> i32;
    fn catalog_apply_scene(scene: *const SceneSnapshot, cached: i32) -> i32;
    fn catalog_apply_scene_view(
        scene: *const SceneSnapshot,
        cached: i32,
        view: *const NativeSceneView,
        revision: u32,
    ) -> i32;
    fn catalog_capture_scene() -> *const SceneSnapshot;
    fn cat_revisao() -> u32;
    fn cat_n() -> i32;
    fn cat_ler_cache(directory: *const c_char) -> i32;
    fn native_title_current() -> *const c_void;
    fn native_title_text(item: *const c_void, field: u32) -> *const c_char;
    fn native_title_publish(item: *const c_void);
    fn native_title_frame();
    fn native_catalog_row_text(index: i32, field: u32) -> *const c_char;
    fn native_title_create(title: *const c_void) -> *mut c_void;
    fn native_title_destroy(title: *mut c_void);
    fn cat_trocar_continuar(items: *const c_void, count: i32);
    fn discovery_test_runtime_catalog(addon_index: usize, catalog_index: usize) -> i32;
    fn native_metadata_copy(index: i32) -> *mut c_void;
    fn native_metadata_try_episode() -> i32;
    fn catalog_metadata_current() -> i32;
    fn catalog_metadata_end();
    fn native_title_update(title: *const c_void);
    fn addon_registry_test_attach(controller: *mut c_void);
    fn addon_registry_reader_current() -> i32;
    fn native_catalog_project_reorder();
    fn catalog_content_begin() -> i32;
    fn catalog_content_end();
}
struct Account {
    context: Mutex<ContentContext>,
    addons: Vec<InstalledAddon>,
    profile_addons: Mutex<HashMap<u32, Vec<InstalledAddon>>>,
}
impl AddonAccount for Account {
    fn current_context(&self, _: bool) -> Option<ContentContext> {
        Some(self.context.lock().unwrap().clone())
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        *self.context.lock().unwrap() == *context
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        if self.is_current(context) {
            Ok(self
                .profile_addons
                .lock()
                .unwrap()
                .get(&context.addons.profile.get())
                .cloned()
                .unwrap_or_else(|| self.addons.clone()))
        } else {
            Err(Error::Cancelled)
        }
    }
    fn write_addons(&self, _: &ContentContext, _: &[InstalledAddon]) -> Result<()> {
        Err(Error::InvalidData)
    }
}
struct Transport {
    body: Mutex<Vec<u8>>,
    urls: Mutex<Vec<Url>>,
    gate: Mutex<(bool, bool)>,
    ready: Condvar,
}
impl ProviderTransport for Transport {
    fn get(&self, url: &Url, _: Duration) -> Result<Response> {
        self.urls.lock().unwrap().push(url.clone());
        let mut gate = self.gate.lock().unwrap();
        if gate.0 {
            gate.1 = true;
            self.ready.notify_all();
            while gate.0 {
                gate = self.ready.wait(gate).unwrap();
            }
        }
        Ok(Response {
            status: 200,
            body: Zeroizing::new(self.body.lock().unwrap().clone()),
        })
    }
}
struct Environment {
    account: Arc<Account>,
    addons: AddonBackend,
    runtime: Arc<CatalogRuntime>,
    transport: Arc<Transport>,
    directory: Directory,
    catalog_id: String,
    _native: Box<NativeAddonController>,
}
impl Environment {
    fn new() -> Self {
        let directory = Directory::new();
        let base = format!(
            "https://provider.invalid/{}/configured",
            "private-config".repeat(220)
        );
        let account = Arc::new(Account {
            context: Mutex::new(context(1, 1)),
            profile_addons: Mutex::new(HashMap::new()),
            addons: InstalledAddon::decode_account_list(
                json!([{"url":base,"enabled":true}]).to_string().as_bytes(),
            )
            .unwrap(),
        });
        let addons = AddonBackend::new(
            account.clone(),
            Arc::new(AddonStore::open(&directory.path).unwrap()),
        );
        let transport = Arc::new(Transport {
            body: Mutex::new(body("profile-one")),
            urls: Mutex::new(Vec::new()),
            gate: Mutex::new((false, false)),
            ready: Condvar::new(),
        });
        let runtime = Arc::new(
            CatalogRuntime::new(
                account.clone(),
                addons.clone(),
                transport.clone(),
                Arc::new(CatalogCache::open(&directory.path).unwrap()),
                Arc::new(SceneCache::open(&directory.path).unwrap()),
            )
            .unwrap(),
        );
        let native = Box::new(NativeAddonController::new(
            AddonController::new(addons.clone()).unwrap(),
        ));
        let env = Self {
            account,
            addons,
            runtime,
            transport,
            directory,
            catalog_id: format!("catalog:{}", "full-id".repeat(60)),
            _native: native,
        };
        env.activate(1);
        unsafe {
            addon_registry_test_attach(
                (&*env._native as *const NativeAddonController)
                    .cast_mut()
                    .cast(),
            );
            catalog_runtime_test_attach(Arc::as_ptr(&env.runtime));
            catalog_runtime_step();
        }
        wait(|| !env.runtime.scenes.needs_restore());
        env
    }
    fn activate(&self, profile: u32) {
        self.select(profile, true);
    }
    fn select(&self, profile: u32, shared: bool) {
        {
            let mut current = self.account.context.lock().unwrap();
            let generation = current.content_generation + 1;
            *current = context(profile, generation);
            if !shared {
                current.addons.profile = ProfileId::new(profile).unwrap();
            }
        }
        let context = self.addons.activate(false).unwrap();
        self.addons.synchronize(&context).unwrap();
        let snapshot = self.addons.snapshot();
        let manifest=Manifest::decode(json!({"id":"provider","name":"Provider","resources":["catalog"],"catalogs":[{"type":"movie","id":self.catalog_id,"name":"Complete catalog"}]}).to_string().as_bytes()).unwrap();
        for record in snapshot.addons.records.iter() {
            self.addons
                .attach_manifest(
                    snapshot.addons.context.as_ref().unwrap(),
                    &record.installed.url,
                    manifest.clone(),
                )
                .unwrap();
        }
    }

    fn job(&self) -> *const NativeCatalogWork {
        unsafe { nuvio_catalog_begin(Arc::as_ptr(&self.runtime)) }
    }
    fn load(&self, work: *const NativeCatalogWork) -> *const SceneSnapshot {
        let mut responded = 0;
        let scene = unsafe { nuvio_catalog_load_index(work, 0, 0, 24, &mut responded) };
        assert!(!scene.is_null());
        scene
    }
    fn publish(&self, work: *const NativeCatalogWork, scene: *const SceneSnapshot) {
        unsafe {
            catalog_runtime_use(work);
            assert_eq!(catalog_apply_scene(scene, 0), 1);
            catalog_runtime_mark_published();
            assert_eq!(catalog_runtime_capture(1), 1);
        }
    }
}
impl Drop for Environment {
    fn drop(&mut self) {
        unsafe {
            catalog_runtime_test_attach(std::ptr::null());
            addon_registry_test_attach(std::ptr::null_mut());
            native_title_publish(std::ptr::null());
            native_title_publish(std::ptr::null());
            native_title_frame();
            native_title_frame();
        }
    }
}
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
fn body(id: &str) -> Vec<u8> {
    json!({"metas":[{"id":id,"imdb_id":"tt123","name":"A title","poster":"https://art.invalid/poster","imdbRating":10}]}).to_string().into_bytes()
}
fn wait(mut condition: impl FnMut() -> bool) {
    let end = Instant::now() + Duration::from_secs(5);
    while !condition() {
        assert!(Instant::now() < end);
        std::thread::sleep(Duration::from_millis(2));
    }
}
unsafe fn current_id() -> String {
    unsafe {
        CStr::from_ptr(native_title_text(
            native_title_current(),
            TitleField::Id as u32,
        ))
    }
    .to_str()
    .unwrap()
    .into()
}

#[test]
fn runtime_catalogs_use_complete_rust_requests_and_round_trip_actual_c_enrichment() {
    let env = Environment::new();
    let work = env.job();
    let id = format!("provider:{}", "complete-id".repeat(300));
    *env.transport.body.lock().unwrap() = body(&id);
    unsafe {
        catalog_runtime_use(work);
        assert_eq!(discovery_test_runtime_catalog(0, 0), 1);
        assert_eq!(catalog_runtime_capture(1), 1);
        assert_eq!(current_id(), id);
        let request = env.transport.urls.lock().unwrap()[0].clone();
        assert!(request.as_str().len() > 2048);
        assert!(request.as_str().contains(&env.catalog_id));
        assert_eq!(
            CStr::from_ptr(native_catalog_row_text(0, 4))
                .to_str()
                .unwrap(),
            env.catalog_id
        );
        let captured = Arc::from_raw(catalog_capture_scene());
        assert_eq!(captured.titles[0].record.metadata.id, id);
        assert_eq!(
            captured.titles[0].record.metadata.imdb_id.as_deref(),
            Some("tt123")
        );
        assert_eq!(
            captured.rows[0].record.source.as_ref().unwrap().catalog_id,
            env.catalog_id
        );
        nuvio_catalog_work_release(work);
    }
    let saved = SceneCache::open(&env.directory.path)
        .unwrap()
        .load(&(&context(1, 1)).into(), &|| true)
        .unwrap()
        .unwrap();
    assert_eq!(saved.titles[0].metadata.id, id);
}

#[test]
fn switching_profiles_hides_old_rows_and_restores_the_selected_private_scene() {
    let env = Environment::new();
    let a = env.job();
    let scene = env.load(a);
    unsafe {
        env.publish(a, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(a);
    }
    env.activate(2);
    unsafe {
        catalog_runtime_step();
        assert_eq!(cat_n(), 0);
        assert_eq!(catalog_runtime_published_current(), 0);
    }
    wait(|| !env.runtime.scenes.needs_restore());
    *env.transport.body.lock().unwrap() = body("profile-two");
    let b = env.job();
    let scene = env.load(b);
    unsafe {
        env.publish(b, scene);
        assert_eq!(current_id(), "profile-two");
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(b);
    }
    env.activate(1);
    unsafe {
        catalog_runtime_step();
        assert_eq!(current_id(), "profile-one");
    }
}

#[test]
fn a_confirmed_removed_manifest_catalog_is_removed_from_the_native_home() {
    let env = Environment::new();
    let job = env.job();
    let scene = env.load(job);
    unsafe {
        env.publish(job, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
    }
    let source = env.addons.snapshot();
    let manifest =
        Manifest::decode(br#"{"id":"provider","name":"Provider","resources":[],"catalogs":[]}"#)
            .unwrap();
    env.addons
        .attach_manifest(
            source.addons.context.as_ref().unwrap(),
            &env.account.addons[0].url,
            manifest,
        )
        .unwrap();
    unsafe {
        catalog_runtime_step();
        assert_eq!(cat_n(), 0);
        nuvio_catalog_work_release(job);
    }
}

#[test]
fn a_catalog_reply_delayed_across_profile_selection_is_cancelled() {
    let env = Environment::new();
    let job = env.job();
    env.transport.gate.lock().unwrap().0 = true;
    let address = unsafe { nuvio_catalog_work_retain(job) } as usize;
    let thread = std::thread::spawn(move || unsafe {
        let work = address as *const NativeCatalogWork;
        catalog_runtime_use(work);
        let count = discovery_test_runtime_catalog(0, 0);
        nuvio_catalog_work_release(work);
        count
    });
    wait(|| env.transport.gate.lock().unwrap().1);
    env.activate(2);
    unsafe {
        catalog_runtime_step();
    }
    env.transport.gate.lock().unwrap().0 = false;
    env.transport.ready.notify_all();
    assert_eq!(thread.join().unwrap(), 0);
    assert_eq!(unsafe { cat_n() }, 0);
    assert_eq!(unsafe { nuvio_catalog_work_current(job) }, 0);
    unsafe {
        nuvio_catalog_work_release(job);
    }
}

#[test]
fn confirmed_empty_catalogs_block_old_row_fallback_while_malformed_replies_keep_valid_cache() {
    let env = Environment::new();
    let job = env.job();
    let first = env.load(job);
    unsafe {
        nuvio_backend::scene_snapshot::nuvio_scene_release(first);
    }
    *env.transport.body.lock().unwrap() = br#"{"metas":[{"id":"broken"}]}"#.to_vec();
    let cached = env.load(job);
    assert_eq!(unsafe { &*cached }.titles.len(), 1);
    unsafe {
        nuvio_backend::scene_snapshot::nuvio_scene_release(cached);
    }
    *env.transport.body.lock().unwrap() = br#"{"metas":[]}"#.to_vec();
    let empty = env.load(job);
    assert!(unsafe { &*empty }.titles.is_empty());
    let source = unsafe { &*empty }.rows[0].record.source.as_ref().unwrap();
    let base = CString::new(source.addon.as_str()).unwrap();
    let kind = CString::new("movie").unwrap();
    let id = CString::new(env.catalog_id.as_str()).unwrap();
    assert_eq!(
        unsafe { nuvio_catalog_row_reusable(job, base.as_ptr(), kind.as_ptr(), id.as_ptr()) },
        0
    );
    unsafe {
        nuvio_backend::scene_snapshot::nuvio_scene_release(empty);
        nuvio_catalog_work_release(job);
    }
}

#[test]
fn restored_row_owners_survive_cw_replacement_and_preserve_extras_in_c_capture() {
    let original = support::scene::complete_scene();
    let scene = Arc::new(SceneSnapshot::new(original.clone()).unwrap());
    let row = Arc::downgrade(&scene.rows[0]);
    unsafe {
        assert_eq!(catalog_apply_scene(Arc::as_ptr(&scene), 1), 1);
        let item = native_title_create(Arc::into_raw(scene.titles[0].clone()).cast());
        drop(scene);
        cat_trocar_continuar(item, 1);
        native_catalog_project_reorder();
        native_title_destroy(item);
        let captured = Arc::from_raw(catalog_capture_scene());
        let restored = captured
            .rows
            .iter()
            .find(|row| row.record.source.is_some())
            .unwrap();
        assert!(restored.record.source == original.rows[0].source);
        assert!(row.upgrade().is_some());
        drop(captured);
        native_title_publish(std::ptr::null());
        native_title_publish(std::ptr::null());
        native_title_frame();
        native_title_frame();
    }
    assert!(row.upgrade().is_none());
}

#[test]
fn a_restored_view_cannot_overwrite_a_newer_c_publication() {
    let env = Environment::new();
    let work = env.job();
    let scene = env.load(work);
    env.runtime
        .scenes
        .accept(&unsafe { &*work }.work, unsafe {
            Arc::increment_strong_count(scene);
            Arc::from_raw(scene)
        })
        .unwrap();
    let view = unsafe { nuvio_catalog_view(Arc::as_ptr(&env.runtime)) };
    let revision = unsafe { cat_revisao() };
    unsafe {
        catalog_apply_scene(scene, 0);
        assert_eq!(catalog_apply_scene_view(scene, 1, view, revision), 0);
        assert_eq!(current_id(), "profile-one");
        nuvio_catalog_view_release(view);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(work);
    }
}

#[test]
fn the_old_c_binary_file_is_ignored_while_only_rust_json_is_saved() {
    let env = Environment::new();
    let old = env.directory.path.join("catalogo-rede.bin");
    std::fs::write(&old, b"original C bytes").unwrap();
    let directory = CString::new(env.directory.path.to_str().unwrap()).unwrap();
    assert_eq!(unsafe { cat_ler_cache(directory.as_ptr()) }, 0);
    let job = env.job();
    let scene = env.load(job);
    unsafe {
        env.publish(job, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(job);
    }
    assert_eq!(std::fs::read(old).unwrap(), b"original C bytes");
    assert!(SceneCache::open(&env.directory.path)
        .unwrap()
        .load(&(&context(1, 1)).into(), &|| true)
        .unwrap()
        .is_some());
}

#[test]
fn logout_clears_only_this_rewrites_scene_and_provider_json() {
    let env = Environment::new();
    let job = env.job();
    let scene = env.load(job);
    unsafe {
        env.publish(job, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
    }
    let context = env.account.context.lock().unwrap().clone();
    let key = nuvio_backend::provider_loader::ProviderLoader::key(
        &context,
        &env.account.addons[0].url,
        ProviderResource::Catalog {
            media_type: "movie".into(),
            id: env.catalog_id.clone(),
            extras: Vec::new(),
        },
    );
    let old = env.directory.path.join("catalogo-rede.bin");
    std::fs::write(&old, b"unrelated old bytes").unwrap();
    env.account.context.lock().unwrap().content_generation += 1;
    env.runtime.logout();
    wait(|| {
        SceneCache::open(&env.directory.path)
            .unwrap()
            .load(&(&context).into(), &|| true)
            .unwrap()
            .is_none()
            && CatalogCache::open(&env.directory.path)
                .unwrap()
                .load(&key)
                .unwrap()
                .is_none()
    });
    assert_eq!(std::fs::read(old).unwrap(), b"unrelated old bytes");
    unsafe {
        nuvio_catalog_work_release(job);
    }
}

#[test]
fn metadata_context_survives_home_refresh_but_cannot_update_the_next_profile() {
    let env = Environment::new();
    *env.transport.body.lock().unwrap() = body("shared-id");
    let job = env.job();
    let scene = env.load(job);
    unsafe {
        env.publish(job, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(job);
    }
    let old = unsafe { native_metadata_copy(0) };
    assert!(!old.is_null());
    assert_eq!(unsafe { addon_registry_reader_current() }, 1);
    let refresh = env.job();
    let scene = env.load(refresh);
    unsafe {
        env.publish(refresh, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(refresh);
        assert_eq!(catalog_metadata_current(), 1);
    }
    env.activate(2);
    unsafe {
        catalog_runtime_step();
    }
    *env.transport.body.lock().unwrap()=json!({"metas":[{"id":"shared-id","name":"New profile title","poster":"https://art.invalid/new-profile"}]}).to_string().into_bytes();
    let next = env.job();
    let scene = env.load(next);
    unsafe {
        env.publish(next, scene);
        assert_eq!(catalog_metadata_current(), 0);
        assert_eq!(addon_registry_reader_current(), 0);
        native_title_update(old);
        assert_eq!(native_metadata_try_episode(), 0);
        assert_eq!(
            CStr::from_ptr(native_title_text(
                native_title_current(),
                TitleField::Name as u32
            ))
            .to_str()
            .unwrap(),
            "New profile title"
        );
        native_title_destroy(old);
        catalog_metadata_end();
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(next);
    }
}

#[test]
fn delayed_continue_watching_cannot_replace_the_selected_profiles_home() {
    let env = Environment::new();
    let first = env.job();
    let scene = env.load(first);
    let old = unsafe { native_title_create(Arc::into_raw((&*scene).titles[0].clone()).cast()) };
    unsafe {
        env.publish(first, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(first);
        assert_eq!(catalog_content_begin(), 1);
    }
    env.activate(2);
    unsafe {
        catalog_runtime_step();
    }
    *env.transport.body.lock().unwrap() = body("profile-two");
    let next = env.job();
    let scene = env.load(next);
    unsafe {
        env.publish(next, scene);
        cat_trocar_continuar(old, 1);
        assert_eq!(cat_n(), 1);
        assert_eq!(current_id(), "profile-two");
        native_title_destroy(old);
        catalog_content_end();
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(next);
    }
}

#[test]
fn a_home_worker_waiting_before_login_uses_the_selected_profiles_arriving_addons() {
    let env = Environment::new();
    let second = InstalledAddon::decode_account_list(
        br#"[{"url":"https://second.invalid/one","name":"Second only"},{"url":"https://second.invalid/two","name":"Second extra"}]"#
    ).unwrap();
    env.account.profile_addons.lock().unwrap().insert(2, second);
    env.addons.forget_runtime();
    let (ready_tx, ready_rx) = std::sync::mpsc::channel();
    let (start_tx, start_rx) = std::sync::mpsc::channel();
    let worker = std::thread::spawn(move || unsafe {
        addon_registry_refresh();
        let before = addons_n();
        ready_tx.send(()).unwrap();
        start_rx.recv_timeout(Duration::from_secs(5)).unwrap();
        catalog_runtime_cycle_begin();
        let count = addons_n();
        let base = CStr::from_ptr(addons_base(0))
            .to_string_lossy()
            .into_owned();
        let current = addon_registry_reader_current();
        let loaded = discovery_test_runtime_catalog(0, 0);
        catalog_runtime_cycle_end();
        (before, count, base, current, loaded)
    });
    ready_rx.recv_timeout(Duration::from_secs(5)).unwrap();
    env.select(2, false);
    *env.transport.body.lock().unwrap() = body("second-profile-first-login");
    unsafe {
        catalog_runtime_step();
    }
    wait(|| !env.runtime.scenes.needs_restore());
    start_tx.send(()).unwrap();
    let (before, count, base, current, loaded) = worker.join().unwrap();
    assert_eq!(before, 0);
    assert_eq!(
        count, 2,
        "the account wait must not leave the empty pre-login snapshot pinned"
    );
    assert_eq!(base, "https://second.invalid/one");
    assert_eq!(current, 1);
    assert_eq!(loaded, 1);
    unsafe {
        assert_eq!(current_id(), "second-profile-first-login");
    }
}

#[test]
fn home_cycles_and_settings_counts_follow_independent_profile_addon_lists() {
    let env = Environment::new();
    env.account.profile_addons.lock().unwrap().insert(2, InstalledAddon::decode_account_list(
        br#"[{"url":"https://second.invalid/one","name":"Second only"},{"url":"https://second.invalid/two","name":"Second extra"}]"#
    ).unwrap());
    for (profile, expected_count, expected_base) in [
        (1, 1, env.account.addons[0].url.as_str()),
        (2, 2, "https://second.invalid/one"),
        (1, 1, env.account.addons[0].url.as_str()),
        (2, 2, "https://second.invalid/one"),
    ] {
        env.select(profile, false);
        let id = format!("profile-{profile}-only");
        *env.transport.body.lock().unwrap() = body(&id);
        unsafe {
            catalog_runtime_step();
            catalog_runtime_cycle_begin();
            assert_eq!(addons_n(), expected_count);
            assert_eq!(
                CStr::from_ptr(addons_base(0)).to_str().unwrap(),
                expected_base
            );
            assert_eq!(addon_registry_reader_current(), 1);
            assert_eq!(discovery_test_runtime_catalog(0, 0), 1);
            assert_eq!(cat_n(), 1);
            assert_eq!(current_id(), id);
            assert_eq!(
                CStr::from_ptr(native_catalog_row_text(0, 3))
                    .to_str()
                    .unwrap(),
                expected_base
            );
            catalog_runtime_cycle_end();
        }
    }
}

#[test]
fn the_native_profile_adapter_and_settings_counts_honor_account_sharing_flags() {
    let directory = Directory::new();
    directory
        .store()
        .save(&nuvio_backend::storage::AccountData {
            session: Some(support::session("member", None, "selected-profile")),
            ..Default::default()
        })
        .unwrap();
    let service = support::service(|request| match request.url.path() {
        "/rest/v1/rpc/get_sync_owner" => support::response(200, json!("owner")),
        "/rest/v1/rpc/sync_pull_profiles" => support::response(
            200,
            json!([
                {"profile_index":1,"name":"Primary","is_primary":true,"uses_primary_addons":false},
                {"profile_index":2,"name":"Independent","uses_primary_addons":false},
                {"profile_index":3,"name":"Sharing","uses_primary_addons":true}
            ]),
        ),
        "/rest/v1/rpc/sync_pull_profile_locks" => support::response(
            200,
            json!([
                {"profile_id":1,"pin_enabled":false}, {"profile_id":2,"pin_enabled":false}, {"profile_id":3,"pin_enabled":false}
            ]),
        ),
        "/rest/v1/addons" => {
            let profile = request
                .url
                .query_pairs()
                .find(|(key, _)| key == "profile_id")
                .unwrap()
                .1
                .into_owned();
            support::response(
                200,
                if profile == "eq.2" {
                    json!([{"url":"https://second.invalid/one"},{"url":"https://second.invalid/two"}])
                } else {
                    assert_eq!(profile, "eq.1");
                    json!([{"url":"https://primary.invalid/one"}])
                },
            )
        }
        _ => panic!("unexpected account fixture request: {}", request.url.path()),
    });
    let account = support::backend(service, &directory);
    account.load_profiles().unwrap();
    let mut native_account =
        Box::new(nuvio_backend::controller::AccountController::new(account.clone()).unwrap());
    let addons = AddonBackend::new(
        Arc::new(account.clone()),
        Arc::new(AddonStore::open(&directory.path).unwrap()),
    );
    let mut native_addons = Box::new(NativeAddonController::new(
        AddonController::new(addons.clone()).unwrap(),
    ));
    unsafe {
        account_ui_test_attach(
            (&mut *native_account as *mut nuvio_backend::controller::AccountController).cast(),
        );
        addon_registry_test_attach((&mut *native_addons as *mut NativeAddonController).cast());
    }
    struct Attachment;
    impl Drop for Attachment {
        fn drop(&mut self) {
            unsafe {
                account_ui_test_attach(std::ptr::null_mut());
                addon_registry_test_attach(std::ptr::null_mut());
            }
        }
    }
    let _attachment = Attachment;
    for (profile, owner, count) in [(2, 2, 2), (1, 1, 1), (2, 2, 2), (3, 1, 1), (2, 2, 2)] {
        account
            .select_profile(ProfileId::new(profile).unwrap(), None)
            .unwrap();
        unsafe {
            assert_eq!(
                profiles_active_addons(),
                owner,
                "native routing must use the server's sharing flag"
            );
        }
        let context = addons.activate(false).unwrap();
        addons.synchronize(&context).unwrap();
        unsafe {
            addon_registry_refresh();
            assert_eq!(addons_n(), count, "settings must count the selected list");
            assert_eq!(addon_registry_reader_current(), 1);
        }
    }
    unsafe {
        account_ui_test_attach(std::ptr::null_mut());
        addon_registry_test_attach(std::ptr::null_mut());
    }
}

struct HomePlanGuard(*mut NativeHomePlan);
impl Drop for HomePlanGuard {
    fn drop(&mut self) {
        unsafe {
            nuvio_home_destroy(self.0);
        }
    }
}
fn home_plan(env: &Environment, work: *const NativeCatalogWork) -> HomePlanGuard {
    std::env::set_var("NUVIO_DADOS", &env.directory.parent);
    unsafe {
        data_init(std::ptr::null());
        fil_definir_perfil(0);
        fil_definir_perfil(env.account.context.lock().unwrap().profile.get() as i32);
    }
    let plan = HomePlanGuard(unsafe { nuvio_home_prepare(work, None, std::ptr::null_mut()) });
    assert!(!plan.0.is_null());
    plan
}

#[test]
fn rust_home_coordination_drives_actual_native_publication_with_complete_values() {
    let env = Environment::new();
    let work = env.job();
    let plan = home_plan(&env, work);
    let plan_path = std::fs::read_dir(env.directory.path.join("rust-backend-v1"))
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .find(|path| {
            path.file_name()
                .unwrap()
                .to_string_lossy()
                .starts_with("home-plan-")
        })
        .expect("the runtime saves declarations before catalog requests");
    let saved: serde_json::Value =
        serde_json::from_slice(&std::fs::read(plan_path).unwrap()).unwrap();
    assert_eq!(
        saved["keys"][0],
        format!("provider_movie_{}", env.catalog_id)
    );
    let id = format!("provider:{}", "complete-identity".repeat(220));
    *env.transport.body.lock().unwrap() = body(&id);
    unsafe {
        assert_eq!(nuvio_home_count(plan.0), 1);
        let text = nuvio_home_text(plan.0, 0, 4);
        assert_eq!(CStr::from_ptr(text).to_str().unwrap(), env.catalog_id);
        nuvio_backend::ffi::nuvio_text_destroy(text);
        let mut stats = NativeLoadStats::default();
        assert_eq!(
            discovery_test_home_loading(
                plan.0,
                work,
                std::ptr::null(),
                std::ptr::null(),
                7,
                &mut stats
            ),
            1
        );
        assert_eq!(stats.rows, 1);
        assert_eq!(stats.requested, 1);
        assert_eq!(current_id(), id);
        assert_eq!(
            CStr::from_ptr(native_catalog_row_text(0, 4))
                .to_str()
                .unwrap(),
            env.catalog_id
        );
        let captured = Arc::from_raw(catalog_capture_scene());
        assert_eq!(
            captured.rows[0].record.source.as_ref().unwrap().catalog_id,
            env.catalog_id
        );
        assert!(env.transport.urls.lock().unwrap()[0].as_str().len() > 2048);
        nuvio_catalog_work_release(work);
    }
}
#[test]
fn rust_home_keeps_continue_watching_and_saved_list_owners_through_native_assembly() {
    let env = Environment::new();
    let work = env.job();
    let plan = home_plan(&env, work);
    let scene = env.load(work);
    unsafe {
        let source = nuvio_backend::scene_snapshot::nuvio_scene_title(scene, 0);
        let continuing =
            native_title_create(nuvio_backend::title_snapshot::nuvio_title_retain(source).cast());
        let saved =
            native_title_create(nuvio_backend::title_snapshot::nuvio_title_retain(source).cast());
        let mut stats = NativeLoadStats::default();
        assert_eq!(
            discovery_test_home_loading(plan.0, work, continuing, saved, 7, &mut stats),
            1
        );
        assert_eq!(stats.rows, 1);
        assert_eq!(
            CStr::from_ptr(native_catalog_row_text(0, 0))
                .to_str()
                .unwrap(),
            "continue_watching"
        );
        assert_eq!(
            CStr::from_ptr(native_catalog_row_text(1, 4))
                .to_str()
                .unwrap(),
            env.catalog_id
        );
        native_title_destroy(continuing);
        native_title_destroy(saved);
        nuvio_backend::title_snapshot::nuvio_title_release(source);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        let captured = Arc::from_raw(catalog_capture_scene());
        assert!(captured.titles.len() >= 2);
        assert_eq!(captured.rows[1].record.titles.start, 1);
        assert_eq!(captured.titles[0].record.metadata.id, "profile-one");
        assert!(captured
            .titles
            .iter()
            .any(|title| title.record.on_watchlist));
        nuvio_catalog_work_release(work);
    }
}
#[test]
fn a_home_plan_is_bound_to_its_profile_and_manifest_declarations() {
    let env = Environment::new();
    let a = env.job();
    let plan = home_plan(&env, a);
    let source = env.addons.snapshot();
    let changed =
        Manifest::decode(br#"{"id":"provider","name":"Provider","resources":[],"catalogs":[]}"#)
            .unwrap();
    env.addons
        .attach_manifest(
            source.addons.context.as_ref().unwrap(),
            &env.account.addons[0].url,
            changed,
        )
        .unwrap();
    unsafe extern "C" fn count_publications(
        _: *const SceneSnapshot,
        _: i32,
        _: *const NativeLoadStats,
        context: *mut c_void,
    ) -> i32 {
        *unsafe { &mut *context.cast::<usize>() } += 1;
        1
    }
    let mut publications = 0usize;
    unsafe {
        assert_eq!(
            nuvio_home_load(
                plan.0,
                a,
                7,
                24,
                Some(count_publications),
                (&mut publications as *mut usize).cast(),
                std::ptr::null_mut()
            ),
            0,
            "a changed manifest must invalidate the old candidate plan"
        );
    }
    assert_eq!(publications, 0);
    env.activate(2);
    unsafe {
        catalog_runtime_step();
    }
    let b = env.job();
    unsafe {
        assert_eq!(
            nuvio_home_load(
                plan.0,
                b,
                7,
                24,
                Some(count_publications),
                (&mut publications as *mut usize).cast(),
                std::ptr::null_mut()
            ),
            0,
            "a foreign profile job cannot reuse the old plan"
        );
        assert_eq!(publications, 0);
        assert_eq!(cat_n(), 0);
        nuvio_catalog_work_release(a);
        nuvio_catalog_work_release(b);
    }
}

#[test]
fn a_delayed_rust_home_publication_is_rejected_after_native_profile_switch() {
    let env = Environment::new();
    let work = env.job();
    let plan = home_plan(&env, work);
    env.transport.gate.lock().unwrap().0 = true;
    let plan_ptr = plan.0 as usize;
    let work_ptr = work as usize;
    let running = std::thread::spawn(move || unsafe {
        let mut stats = NativeLoadStats::default();
        discovery_test_home_loading(
            plan_ptr as *mut NativeHomePlan,
            work_ptr as *const NativeCatalogWork,
            std::ptr::null(),
            std::ptr::null(),
            7,
            &mut stats,
        )
    });
    wait(|| env.transport.gate.lock().unwrap().1);
    env.activate(2);
    unsafe {
        catalog_runtime_step();
        assert_eq!(cat_n(), 0);
    }
    {
        let mut gate = env.transport.gate.lock().unwrap();
        gate.0 = false;
        env.transport.ready.notify_all();
    }
    assert_eq!(running.join().unwrap(), 0);
    unsafe {
        assert_eq!(cat_n(), 0);
        nuvio_catalog_work_release(work);
    }
}

#[test]
fn the_home_row_editor_rejects_registration_from_an_older_profile_activation() {
    let env = Environment::new();
    let work = env.job();
    let _plan = home_plan(&env, work);
    unsafe {
        let generation = fil_perfil_geracao();
        fil_definir_perfil(2);
        let before = fil_n();
        let text = CString::new("old-profile-row").unwrap();
        assert_eq!(
            register_profile_row(
                1,
                generation,
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                0,
                0
            ),
            0
        );
        assert_eq!(fil_n(), before);
        fil_definir_perfil(1);
        assert_eq!(
            register_profile_row(
                1,
                generation,
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                0,
                0
            ),
            0
        );
        let current = fil_n();
        assert_eq!(
            register_profile_row(
                1,
                fil_perfil_geracao(),
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                text.as_ptr(),
                0,
                0
            ),
            1
        );
        assert_eq!(fil_n(), current + 1);
        nuvio_catalog_work_release(work);
    }
}

#[test]
fn a_fresh_rust_home_fetches_and_parses_the_manifest_before_catalog_loading() {
    let env = Environment::new();
    env.addons.forget_runtime();
    let context = env.addons.activate(false).unwrap();
    env.addons.synchronize(&context).unwrap();
    *env.transport.body.lock().unwrap()=json!({"id":"provider","name":"Provider","resources":["catalog"],"catalogs":[{"type":"movie","id":env.catalog_id,"name":"Complete catalog"}]}).to_string().into_bytes();
    unsafe {
        catalog_runtime_step();
    }
    let work = env.job();
    let plan = home_plan(&env, work);
    assert_eq!(unsafe { nuvio_home_count(plan.0) }, 1);
    assert!(env
        .transport
        .urls
        .lock()
        .unwrap()
        .iter()
        .any(|url| url.path().ends_with("manifest.json")));
    *env.transport.body.lock().unwrap() = body("fresh-home-title");
    unsafe {
        let mut stats = NativeLoadStats::default();
        assert_eq!(
            discovery_test_home_loading(
                plan.0,
                work,
                std::ptr::null(),
                std::ptr::null(),
                7,
                &mut stats
            ),
            1
        );
        assert_eq!(current_id(), "fresh-home-title");
        nuvio_catalog_work_release(work);
    }
}
#[test]
fn a_manifest_outage_preserves_the_private_home_scene_through_the_native_adapter() {
    let env = Environment::new();
    let first = env.job();
    let scene = env.load(first);
    unsafe {
        env.publish(first, scene);
        nuvio_backend::scene_snapshot::nuvio_scene_release(scene);
        nuvio_catalog_work_release(first);
    }
    env.addons.forget_runtime();
    let context = env.addons.activate(false).unwrap();
    env.addons.synchronize(&context).unwrap();
    *env.transport.body.lock().unwrap() = b"malformed manifest".to_vec();
    env.transport.urls.lock().unwrap().clear();
    unsafe {
        catalog_runtime_step();
    }
    let work = env.job();
    let plan = home_plan(&env, work);
    unsafe {
        let mut stats = NativeLoadStats::default();
        assert_eq!(
            discovery_test_home_loading(
                plan.0,
                work,
                std::ptr::null(),
                std::ptr::null(),
                7,
                &mut stats
            ),
            1
        );
        assert_eq!(stats.rows, 1);
        assert_eq!(current_id(), "profile-one");
        assert!(env
            .transport
            .urls
            .lock()
            .unwrap()
            .iter()
            .all(|url| url.path().ends_with("manifest.json")));
        nuvio_catalog_work_release(work);
    }
}
#[test]
fn native_channel_registration_does_not_enable_unselected_tv_rows_on_later_refreshes() {
    let env = Environment::new();
    let source = env.addons.snapshot();
    let manifest=Manifest::decode(json!({"id":"provider","resources":["catalog"],"catalogs":[{"type":"channel","id":"live","name":"TV channels"},{"type":"movie","id":env.catalog_id,"name":"Movies"}]}).to_string().as_bytes()).unwrap();
    env.addons
        .attach_manifest(
            source.addons.context.as_ref().unwrap(),
            &env.account.addons[0].url,
            manifest,
        )
        .unwrap();
    for _ in 0..2 {
        let work = env.job();
        let plan = home_plan(&env, work);
        unsafe {
            let mut stats = NativeLoadStats::default();
            assert_eq!(
                discovery_test_home_loading(
                    plan.0,
                    work,
                    std::ptr::null(),
                    std::ptr::null(),
                    7,
                    &mut stats
                ),
                1
            );
            assert_eq!(stats.rows, 1);
            assert_eq!(
                CStr::from_ptr(native_catalog_row_text(0, 4))
                    .to_str()
                    .unwrap(),
                env.catalog_id
            );
            nuvio_catalog_work_release(work);
        }
    }
    assert!(env
        .transport
        .urls
        .lock()
        .unwrap()
        .iter()
        .all(|url| !url.path().contains("/channel/")));
}
