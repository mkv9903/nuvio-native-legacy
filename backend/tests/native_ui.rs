#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    account::{AccountBackend, LoginState},
    controller::AccountController,
    ffi::nuvio_account_destroy,
};
use serde_json::{json, Value};
use std::{
    ffi::{c_char, c_void, CString},
    fs,
    path::Path,
    process::Command,
    sync::{
        atomic::{AtomicU64, AtomicUsize, Ordering},
        Arc, Barrier,
    },
    thread,
    time::{Duration, Instant},
};
use support::*;

#[repr(C)]
#[derive(Default)]
struct PickerState {
    focus: [f32; 8],
    pin: f32,
    light: [f32; 6],
    light_target: [f32; 6],
    fade: [f32; 9],
    wall_time: f32,
    burst_time: f32,
    wall_count: i32,
    particles: i32,
    burst: i32,
    burst_drawn: i32,
    ambient_fade: f32,
    ambient_current: i32,
    ambient_previous: i32,
    ambient_source: i32,
    continue_available: [i32; 8],
    pin_slot: i32,
    pin_focus: i32,
    verifying: i32,
    pin_digits: i32,
    complete: i32,
}
#[link(name = "nuvio_c_ui")]
extern "C" {
    fn rust_addons_test_view_size(kind: i32) -> usize;
    fn rust_addons_test_view_offset(kind: i32, field: usize) -> usize;
    fn account_ui_test_attach(account: *mut c_void);
    fn addon_registry_test_attach(controller: *mut c_void);
    fn addon_registry_refresh();
    fn addon_screen_open();
    fn addon_screen_update(dt: f32, now: u32);
    fn addon_screen_draw(now: u32);
    fn native_addons_test_key(key: i32);
    fn data_init(directory: *const c_char);
    fn ajustes_dir(directory: *const c_char);
    fn session_step(now: u32);
    fn login_set_assets(directory: *const c_char);
    fn login_init();
    fn login_update(dt: f32, now: u32);
    fn login_draw(now: u32);
    fn login_release();
    fn login_test_email_keyboard_start();
    fn login_test_sign_in();
    fn login_test_qr_ready() -> i32;
    fn st_teste_ligar(enabled: i32);
    fn st_teste_evento(event: *const c_char);
    fn logoapp_iniciar(directory: *const c_char);
    fn iconeapp_iniciar(directory: *const c_char);
    fn profile_picker_init();
    fn profile_picker_update(dt: f32, now: u32);
    fn profile_picker_draw(now: u32);
    fn profile_picker_prepare(enabled: i32, now: u32);
    fn profile_picker_test_state(state: *mut PickerState);
    fn profile_picker_test_key(key: i32);
    fn profile_picker_test_activate(slot: i32, id: i32);
    fn profile_picker_test_focus_pin(key: i32);
    fn gfx_iniciar() -> i32;
    fn gfx_tamanho_alvo(width: i32, height: i32);
    fn gfx_novo_quadro();
    fn gfx_encerrar();
    fn gfx_icones_dir(directory: *const c_char);
    fn txt_iniciar(directory: *const c_char, scale: f32) -> i32;
    fn txt_novo_quadro();
    fn txt_encerrar();
    fn tex_iniciar(memory_mb: i32);
    fn tex_novo_quadro();
    fn tex_bombear(count: i32);
    fn tex_encerrar();
}

#[test]
fn addon_native_layout_matches_the_c_consumers() {
    use nuvio_backend::addon_snapshot::{AddonSnapshotView, AddonView, CatalogView};
    use std::mem::{offset_of, size_of};
    let layouts = [
        (
            size_of::<AddonSnapshotView>(),
            vec![
                offset_of!(AddonSnapshotView, revision),
                offset_of!(AddonSnapshotView, list_revision),
                offset_of!(AddonSnapshotView, addons),
                offset_of!(AddonSnapshotView, count),
                offset_of!(AddonSnapshotView, profile),
                offset_of!(AddonSnapshotView, addon_profile),
                offset_of!(AddonSnapshotView, authoritative),
                offset_of!(AddonSnapshotView, pending),
                offset_of!(AddonSnapshotView, status),
                offset_of!(AddonSnapshotView, error),
            ],
        ),
        (
            size_of::<AddonView>(),
            vec![
                offset_of!(AddonView, name),
                offset_of!(AddonView, base),
                offset_of!(AddonView, id),
                offset_of!(AddonView, enabled),
                offset_of!(AddonView, known),
                offset_of!(AddonView, catalog),
                offset_of!(AddonView, stream),
                offset_of!(AddonView, subtitles),
                offset_of!(AddonView, metadata),
                offset_of!(AddonView, silent_queries),
                offset_of!(AddonView, catalogs),
                offset_of!(AddonView, catalog_count),
            ],
        ),
        (
            size_of::<CatalogView>(),
            vec![
                offset_of!(CatalogView, id),
                offset_of!(CatalogView, media_type),
                offset_of!(CatalogView, name),
                offset_of!(CatalogView, searchable),
                offset_of!(CatalogView, requires_search),
                offset_of!(CatalogView, channel),
            ],
        ),
    ];
    for (kind, (size, offsets)) in layouts.into_iter().enumerate() {
        assert_eq!(unsafe { rust_addons_test_view_size(kind as i32) }, size);
        for (field, offset) in offsets.into_iter().enumerate() {
            assert_eq!(
                unsafe { rust_addons_test_view_offset(kind as i32, field) },
                offset
            );
        }
    }
}
#[link(name = "SDL2")]
extern "C" {
    fn SDL_Init(flags: u32) -> i32;
    fn SDL_GL_SetAttribute(attribute: i32, value: i32) -> i32;
    fn SDL_CreateWindow(
        title: *const c_char,
        x: i32,
        y: i32,
        width: i32,
        height: i32,
        flags: u32,
    ) -> *mut c_void;
    fn SDL_GL_CreateContext(window: *mut c_void) -> *mut c_void;
    fn SDL_GL_SwapWindow(window: *mut c_void);
    fn SDL_GL_SetSwapInterval(interval: i32) -> i32;
    fn SDL_GetTicks() -> u32;
    fn SDL_GL_DeleteContext(context: *mut c_void);
    fn SDL_DestroyWindow(window: *mut c_void);
    fn SDL_Quit();
}
#[link(name = "SDL2_image")]
extern "C" {
    fn IMG_Init(flags: i32) -> i32;
}
#[link(name = "GLESv2")]
extern "C" {
    fn glViewport(x: i32, y: i32, width: i32, height: i32);
}

fn picker_state() -> PickerState {
    let mut state = PickerState::default();
    unsafe {
        profile_picker_test_state(&mut state);
    }
    state
}
fn wait_until(mut condition: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(5);
    while !condition() {
        assert!(Instant::now() < deadline, "UI action did not finish");
        thread::sleep(Duration::from_millis(2));
    }
}
fn frames(window: *mut c_void, picker: bool) {
    unsafe {
        for _ in 0..90 {
            let now = SDL_GetTicks();
            session_step(now);
            txt_novo_quadro();
            tex_novo_quadro();
            tex_bombear(6);
            gfx_novo_quadro();
            if picker {
                profile_picker_update(1.0 / 60.0, now);
                profile_picker_draw(now);
            } else {
                login_update(1.0 / 60.0, now);
                login_draw(now);
            }
            SDL_GL_SwapWindow(window);
        }
    }
}
fn capture(name: &str) {
    let directory = std::env::var_os("NUVIO_UI_TEST_CAPTURE_DIR").expect("capture directory");
    assert!(Command::new("import")
        .arg("-window")
        .arg("root")
        .arg(Path::new(&directory).join(name))
        .status()
        .unwrap()
        .success());
}

fn set_reduced_motion(directory: &Directory, enabled: bool) {
    // Settings remain a C capability; this is its existing file protocol.
    fs::write(
        directory.path.join("ajustes.txt"),
        format!("animacoes {}\n", i32::from(enabled)),
    )
    .unwrap();
    let path = CString::new(directory.path.to_str().unwrap()).unwrap();
    unsafe { ajustes_dir(path.as_ptr()) };
}

fn check_preserved_animations(directory: &Directory) {
    unsafe {
        let before = picker_state();
        profile_picker_test_key(1073741903);
        profile_picker_update(10.0, SDL_GetTicks());
        let after = picker_state();
        assert!(after.focus[0] < before.focus[0]);
        assert!(after.focus[1] > before.focus[1] && after.focus[1] < 1.0);
        assert!(after.wall_time - before.wall_time <= 0.0501);
        assert!(after.light[0] > before.light[0] && after.light[0] < after.light_target[0]);
        for _ in 0..6 {
            profile_picker_update(0.05, SDL_GetTicks());
        }
        let settled = picker_state();
        assert!((settled.light[0] - settled.light_target[0]).abs() < 0.002);

        // Sustained slow frames reduce effects once, retaining the reduction
        // when frame times recover, as in the original C visual regression.
        for _ in 0..30 {
            profile_picker_update(0.04, SDL_GetTicks());
        }
        assert_eq!((picker_state().particles, picker_state().burst), (20, 18));
        for _ in 0..30 {
            profile_picker_update(1.0 / 60.0, SDL_GetTicks());
        }
        assert_eq!((picker_state().particles, picker_state().burst), (20, 18));

        set_reduced_motion(directory, true);
        profile_picker_test_key(1073741903);
        profile_picker_update(0.04, SDL_GetTicks());
        profile_picker_draw(SDL_GetTicks());
        let reduced = picker_state();
        assert_eq!(reduced.focus[2], 1.0);
        for (light, target) in reduced.light.iter().zip(reduced.light_target) {
            assert!((light - target).abs() < 0.0001);
        }
        assert_eq!(
            (reduced.wall_time, reduced.burst_time, reduced.burst_drawn),
            (0.0, 0.0, 0)
        );
        set_reduced_motion(directory, false);
        profile_picker_init();
    }
}

#[test]
fn original_c_screens_use_rust_login_selection_pin_and_remote_actions() {
    let root = Path::new(env!("CARGO_MANIFEST_DIR")).parent().unwrap();
    std::env::set_current_dir(root).unwrap();
    let directory = Directory::new();
    std::env::set_var("NUVIO_DATA", &directory.parent);
    let clock = Arc::new(TestClock(AtomicU64::new(1000)));
    let pins = Arc::new(AtomicUsize::new(0));
    let pin_requests = pins.clone();
    let pause_pin = Arc::new(AtomicUsize::new(0));
    let mode = pause_pin.clone();
    let pin_started = Arc::new(Barrier::new(2));
    let started = pin_started.clone();
    let pin_release = Arc::new(Barrier::new(2));
    let release = pin_release.clone();
    let service = service(move |request| match request.url.path() {
        "/auth/v1/signup" => response(200, auth("anonymous", None, "anon")),
        "/rest/v1/rpc/start_tv_login_session" => response(
            200,
            json!([{"code":"0123456789abcdef0123456789abcdef","web_url":"https://login.invalid/tv/test","poll_interval_seconds":3}]),
        ),
        "/rest/v1/rpc/poll_tv_login_session" => response(200, json!([{"status":"authorized"}])),
        "/functions/v1/tv-logins-exchange" => response(200, auth("user", None, "user")),
        "/auth/v1/token" => {
            let body: Value = serde_json::from_slice(&request.body).unwrap();
            assert_eq!(body["email"], "person@example.test");
            assert_eq!(body["password"], " MiX pAss\"word\\! #9 ");
            response(200, auth("user", None, "user"))
        }
        "/rest/v1/rpc/get_sync_owner" => response(200, json!("owner")),
        "/rest/v1/addons" => response(
            200,
            json!([
                {"url":"https://first.invalid/manifest.json","name":"Catalog addon","enabled":true},
                {"url":"https://second.invalid/manifest.json","name":"Source addon","enabled":true},
                {"url":"https://third.invalid/manifest.json","name":"Subtitle addon","enabled":false}
            ]),
        ),
        "/rest/v1/rpc/sync_push_addons" => response(200, json!(null)),
        "/rest/v1/rpc/sync_pull_profiles" => response(
            200,
            json!([
            {"profile_index":1,"id":"primary-database-key","name":"Primary","avatar_color_hex":"#1E88E5"},
            {"profile_index":2,"name":"Second","avatar_color_hex":"#E53935","uses_primary_addons":true},
            {"profile_index":3,"name":"Kids"}]),
        ),
        "/rest/v1/rpc/sync_pull_profile_locks" => response(
            200,
            json!([
            {"profile_id":"primary-database-key","profile_index":1,"pin_enabled":false},{"profile_id":2,"pin_enabled":false},{"profile_id":3,"pin_enabled":true}]),
        ),
        "/rest/v1/rpc/verify_profile_pin" => {
            let body: Value = serde_json::from_slice(&request.body).unwrap();
            assert_eq!(body["p_profile_id"], 3);
            pin_requests.fetch_add(1, Ordering::SeqCst);
            if mode.load(Ordering::SeqCst) != 0 {
                started.wait();
                release.wait();
            }
            response(200, json!(body["p_pin"] == "4271"))
        }
        _ => panic!("unexpected UI fixture request"),
    });
    let backend = AccountBackend::new(service, Some(directory.store()), clock.clone());
    let handle = Box::into_raw(Box::new(AccountController::new(backend.clone()).unwrap()));
    unsafe {
        assert_eq!(SDL_Init(0x21), 0);
        IMG_Init(3);
        SDL_GL_SetAttribute(21, 4);
        SDL_GL_SetAttribute(17, 2);
        SDL_GL_SetAttribute(18, 0);
        let title = CString::new("Nuvio Rust Test UI regression").unwrap();
        let window = SDL_CreateWindow(title.as_ptr(), 0x2fff0000, 0x2fff0000, 1920, 1080, 6);
        assert!(!window.is_null());
        let context = SDL_GL_CreateContext(window);
        assert!(!context.is_null());
        SDL_GL_SetSwapInterval(0);
        glViewport(0, 0, 1920, 1080);
        gfx_tamanho_alvo(1920, 1080);
        assert_ne!(gfx_iniciar(), 0);
        let resources = CString::new("deploy/app").unwrap();
        assert_ne!(txt_iniciar(resources.as_ptr(), 1.0), 0);
        tex_iniciar(64);
        let artwork = CString::new("deploy/app/art").unwrap();
        gfx_icones_dir(artwork.as_ptr());
        data_init(std::ptr::null());
        account_ui_test_attach(handle.cast());
        logoapp_iniciar(artwork.as_ptr());
        iconeapp_iniciar(artwork.as_ptr());
        login_set_assets(artwork.as_ptr());
        login_init();
        wait_until(|| {
            session_step(SDL_GetTicks());
            backend.snapshot().login == LoginState::Waiting
        });
        frames(window, false);
        assert_ne!(login_test_qr_ready(), 0);
        capture("login-qr.png");
        // Complete the array-shaped QR poll/exchange through the UI worker.
        clock.0.store(1003, Ordering::SeqCst);
        wait_until(|| {
            session_step(SDL_GetTicks());
            backend.snapshot().login == LoginState::LoggedIn
        });
        backend.logout();
        session_step(SDL_GetTicks());
        // Drive field completion through the retained keyboard and login
        // update path. Do not bypass the input handoff with static field writes.
        st_teste_ligar(1);
        login_test_email_keyboard_start();
        st_teste_evento(CString::new("Dperson@example.test").unwrap().as_ptr());
        login_update(1.0 / 60.0, SDL_GetTicks());
        st_teste_evento(CString::new("D MiX pAss\"word\\! #9 ").unwrap().as_ptr());
        login_update(1.0 / 60.0, SDL_GetTicks());
        login_test_sign_in();
        st_teste_ligar(0);
        wait_until(|| {
            session_step(SDL_GetTicks());
            backend.snapshot().login == LoginState::LoggedIn
        });
        frames(window, false);
        capture("login-email.png");
        backend.load_profiles().unwrap();
        session_step(SDL_GetTicks());
        profile_picker_init();
        frames(window, true);
        capture("profile-picker.png");
        check_preserved_animations(&directory);

        // Touching a protected profile opens its PIN pad and keeps the active
        // profile unchanged. A stale slot/identity cannot select another user.
        profile_picker_test_activate(2, 3);
        assert_eq!(picker_state().pin_slot, 2);
        assert_eq!(backend.active_id(), 1);
        let before_pin = picker_state();
        profile_picker_update(0.016, SDL_GetTicks());
        let after_pin = picker_state();
        assert!(after_pin.pin > before_pin.pin && after_pin.pin < 1.0);
        assert_eq!(after_pin.wall_time, before_pin.wall_time);
        profile_picker_test_activate(1, 2);
        assert_eq!(picker_state().pin_slot, 2);
        profile_picker_test_focus_pin(0);
        profile_picker_test_key(13);
        assert_eq!(picker_state().pin_digits, 1);
        profile_picker_test_focus_pin(10);
        profile_picker_test_key(13);
        assert_eq!(picker_state().pin_digits, 2);
        profile_picker_test_focus_pin(11);
        profile_picker_test_key(13);
        assert_eq!(picker_state().pin_digits, 1);
        profile_picker_test_key(27);
        profile_picker_test_key(27);
        assert_eq!(picker_state().pin_slot, -1);
        profile_picker_test_activate(2, 3);
        for key in [b'1', b'2', b'3'] {
            profile_picker_test_key(i32::from(key));
        }
        assert_eq!(pins.load(Ordering::SeqCst), 0);
        profile_picker_test_key(8);
        assert_eq!(picker_state().pin_digits, 2);
        profile_picker_test_key(i32::from(b'3'));
        profile_picker_test_focus_pin(10);
        profile_picker_test_key(13);
        assert_ne!(picker_state().verifying, 0);
        wait_until(|| {
            session_step(SDL_GetTicks());
            profile_picker_update(1.0 / 60.0, SDL_GetTicks());
            picker_state().verifying == 0
        });
        assert_eq!(pins.load(Ordering::SeqCst), 1);
        assert_eq!(backend.active_id(), 1);
        frames(window, true);
        capture("profile-pin-error.png");

        // Blank pad cell is skipped by remote and pointer focus.
        profile_picker_test_focus_pin(6);
        profile_picker_test_key(1073741905);
        assert_eq!(picker_state().pin_focus, 10);
        profile_picker_test_key(1073741904);
        assert_eq!(picker_state().pin_focus, 10);
        profile_picker_test_focus_pin(9);
        assert_eq!(picker_state().pin_focus, 10);
        profile_picker_test_key(1073741903);
        assert_eq!(picker_state().pin_focus, 11);

        // A late correct PIN after Back must not change the active profile.
        pause_pin.store(1, Ordering::SeqCst);
        for key in b"4271" {
            profile_picker_test_key(i32::from(*key));
        }
        pin_started.wait();
        profile_picker_test_key(27);
        pin_release.wait();
        wait_until(|| backend.snapshot().selection_result.is_none());
        assert_eq!(backend.active_id(), 1);
        pause_pin.store(0, Ordering::SeqCst);
        profile_picker_init();
        profile_picker_test_activate(2, 3);
        for key in b"4271" {
            profile_picker_test_key(i32::from(*key));
        }
        wait_until(|| {
            session_step(SDL_GetTicks());
            profile_picker_update(1.0 / 60.0, SDL_GetTicks());
            picker_state().complete != 0
        });
        assert_eq!(backend.active_id(), 3);
        profile_picker_init();
        profile_picker_prepare(1, SDL_GetTicks());
        profile_picker_test_activate(1, 2);
        assert_eq!(backend.active_id(), 3);
        profile_picker_prepare(0, SDL_GetTicks());
        profile_picker_test_activate(1, 99);
        assert_eq!(backend.active_id(), 3);
        profile_picker_test_activate(1, 2);
        wait_until(|| {
            session_step(SDL_GetTicks());
            profile_picker_update(1.0 / 60.0, SDL_GetTicks());
            picker_state().complete != 0
        });
        assert_eq!(backend.active_id(), 2);
        assert_eq!(
            backend
                .snapshot()
                .selection
                .addon_profile(false)
                .unwrap()
                .get(),
            1
        );
        {
            use nuvio_backend::{addon_ffi::*, addon_snapshot::*};
            let path = CString::new(directory.path.to_str().unwrap()).unwrap();
            let addons = nuvio_addons_create(handle, path.as_ptr());
            assert!(!addons.is_null());
            addon_registry_test_attach(addons.cast());
            nuvio_addons_step(addons, 0);
            wait_until(|| {
                (*addons).controller.backend.snapshot().status
                    == nuvio_backend::addon_backend::AddonStatus::Ready
            });
            let snapshot = nuvio_addons_snapshot(addons);
            for (index, resource) in ["catalog", "stream", "subtitles"].iter().enumerate() {
                let body = CString::new(format!(
                    r#"{{"id":"provider{index}","resources":["{resource}"],"types":["movie"]}}"#
                ))
                .unwrap();
                assert_eq!(
                    nuvio_addons_attach_manifest(addons, snapshot, index, body.as_ptr()),
                    1
                );
            }
            nuvio_addons_snapshot_destroy(snapshot);
            addon_registry_refresh();
            addon_screen_open();
            for _ in 0..30 {
                let now = SDL_GetTicks();
                txt_novo_quadro();
                tex_novo_quadro();
                tex_bombear(6);
                gfx_novo_quadro();
                addon_screen_update(1.0 / 60.0, now);
                addon_screen_draw(now);
                SDL_GL_SwapWindow(window);
            }
            capture("addons-rust-backed.png");
            native_addons_test_key(13);
            assert!(
                !(*addons).controller.backend.snapshot().addons.records[0]
                    .installed
                    .enabled
            );
            wait_until(|| !(*addons).controller.backend.snapshot().addons.pending);
            // A damaged offline journal must leave the cached list visible and
            // block the real remote action, without changing screen geometry.
            let state = (*addons).controller.backend.snapshot();
            let scope = state.addons.context.as_ref().unwrap().addons.clone();
            let store = nuvio_backend::addon_storage::AddonStore::open(&directory.path).unwrap();
            use nuvio_backend::{
                addon_edits::{AddonChange, AddonEdit},
                addon_storage::AddonStorage,
            };
            store
                .save_edit(
                    &scope,
                    &AddonEdit::new(AddonChange::SetEnabled {
                        url: state.addons.records[0].installed.url.clone(),
                        enabled: true,
                    }),
                )
                .unwrap();
            let journal = fs::read_dir(directory.path.join("rust-backend-v1"))
                .unwrap()
                .map(|entry| entry.unwrap().path())
                .find(|path| {
                    path.file_name()
                        .unwrap()
                        .to_string_lossy()
                        .starts_with("addons-edit-")
                        && path
                            .extension()
                            .is_some_and(|extension| extension == "json")
                })
                .unwrap();
            fs::write(&journal, "{damaged").unwrap();
            addon_registry_test_attach(std::ptr::null_mut());
            nuvio_addons_destroy(addons);
            let paused = nuvio_addons_create(handle, path.as_ptr());
            assert!(!paused.is_null());
            addon_registry_test_attach(paused.cast());
            nuvio_addons_step(paused, 0);
            wait_until(|| {
                (*paused).controller.backend.snapshot().status
                    == nuvio_backend::addon_backend::AddonStatus::RecoveryPaused
            });
            let snapshot = nuvio_addons_snapshot(paused);
            for (index, resource) in ["catalog", "stream", "subtitles"].iter().enumerate() {
                let body = CString::new(format!(
                    r#"{{"id":"provider{index}","resources":["{resource}"],"types":["movie"]}}"#
                ))
                .unwrap();
                assert_eq!(
                    nuvio_addons_attach_manifest(paused, snapshot, index, body.as_ptr()),
                    1
                );
            }
            nuvio_addons_snapshot_destroy(snapshot);
            addon_registry_refresh();
            addon_screen_open();
            native_addons_test_key(13);
            let state = (*paused).controller.backend.snapshot();
            assert_eq!(state.addons.records.len(), 3);
            assert!(!state.addons.records[0].installed.enabled);
            assert_eq!(
                state.status,
                nuvio_backend::addon_backend::AddonStatus::RecoveryPaused
            );
            assert_eq!(fs::read_to_string(&journal).unwrap(), "{damaged");
            for _ in 0..30 {
                let now = SDL_GetTicks();
                txt_novo_quadro();
                tex_novo_quadro();
                tex_bombear(6);
                gfx_novo_quadro();
                addon_screen_update(1.0 / 60.0, now);
                addon_screen_draw(now);
                SDL_GL_SwapWindow(window);
            }
            capture("addons-recovery-paused.png");
            addon_registry_test_attach(std::ptr::null_mut());
            nuvio_addons_destroy(paused);
        }
        login_release();
        tex_encerrar();
        txt_encerrar();
        gfx_encerrar();
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        nuvio_account_destroy(handle);
    }
}
