mod support;
use nuvio_rust_app::{
    account::{BackendConfig, CloudClient, SessionVault},
    fixtures,
    http::Transport,
    secret::Secret,
    ui::app::{App, Input, Page},
};
use std::{
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc, Arc, Mutex,
    },
    time::{Duration, Instant},
};
use support::{fake, response, tokens, FixtureDirectory};

fn app(directory: &FixtureDirectory, transport: Arc<dyn Transport>) -> (App, Arc<SessionVault>) {
    let vault = Arc::new(SessionVault::new(directory.store()).unwrap());
    let config = Arc::new(
        BackendConfig::new(
            "https://backend.example.invalid",
            "fixture-anon",
            "https://login.example.invalid/tv",
        )
        .unwrap(),
    );
    let cloud = Arc::new(CloudClient::new(config, transport.clone(), vault.clone()));
    (App::new(cloud, transport), vault)
}
fn wait(app: &mut App, ready: impl Fn(&App) -> bool) {
    let deadline = Instant::now() + Duration::from_secs(3);
    loop {
        app.tick();
        if ready(app) {
            return;
        }
        assert!(Instant::now() < deadline, "controller fixture timed out");
        std::thread::sleep(Duration::from_millis(1));
    }
}
fn sign_in(app: &mut App) {
    app.submit_password("fixture@example.invalid", Secret::new("fixture-password"));
    wait(app, |app| app.page == Page::Profiles);
}
fn home_ready(app: &App) -> bool {
    !app.catalogs_loading()
        && !app.home.rows.is_empty()
        && app.home.rows.iter().all(|row| row.loaded)
}

#[test]
fn primary_shared_and_independent_profiles_have_fresh_home_rows() {
    let directory = FixtureDirectory::new();
    let (mut app, _) = app(&directory, fixtures::transport());
    sign_in(&mut app);
    app.select_profile(0);
    wait(&mut app, home_ready);
    assert_eq!(app.home.rows.len(), 2);
    app.select_profile(1);
    assert!(app.home.rows.is_empty());
    wait(&mut app, home_ready);
    assert_eq!(app.home.rows.len(), 1);
    assert_eq!(app.home.rows[0].catalog.kind, "series");
    app.select_profile(2);
    assert!(app.home.rows.is_empty());
    wait(&mut app, home_ready);
    assert!(app
        .home
        .rows
        .iter()
        .all(|row| row.name.starts_with("Independent")));
}

#[test]
fn movie_and_episode_sources_require_an_explicit_selection() {
    let directory = FixtureDirectory::new();
    let (mut app, _) = app(&directory, fixtures::transport());
    sign_in(&mut app);
    app.select_profile(0);
    wait(&mut app, home_ready);
    app.input(Input::Accept);
    wait(&mut app, |app| !app.detail_loading);
    app.input(Input::Accept);
    wait(&mut app, |app| {
        app.sources_loading == 0 && !app.sources.is_empty()
    });
    assert!(app.take_playback().is_none());
    app.input(Input::Down);
    app.input(Input::Accept);
    let selection = app.take_playback().unwrap();
    assert_eq!(selection.title.kind, "movie");
    assert!(selection.choice.source.title.contains("720p"));
    app.open_profiles();
    app.select_profile(1);
    wait(&mut app, home_ready);
    app.input(Input::Accept);
    wait(&mut app, |app| !app.detail_loading);
    app.input(Input::Down);
    app.input(Input::Accept);
    wait(&mut app, |app| {
        app.sources_loading == 0 && !app.sources.is_empty()
    });
    assert!(app.take_playback().is_none());
    app.input(Input::Accept);
    assert!(app.take_playback().unwrap().video_id.ends_with(":1:2"));
}

#[test]
fn all_source_providers_run_even_when_they_exceed_worker_queue_capacity() {
    let directory = FixtureDirectory::new();
    let (mut app, _) = app(&directory, fixtures::transport());
    sign_in(&mut app);
    app.select_profile(0);
    wait(&mut app, home_ready);
    app.input(Input::Accept);
    wait(&mut app, |app| !app.detail_loading);
    let addon = app.addons[0].clone();
    app.addons.clear();
    for index in 0..40 {
        let mut record = (*addon.record).clone();
        record.url = format!("https://addon.example.invalid/provider-{index}/manifest.json");
        app.addons.push(nuvio_rust_app::catalog::Addon {
            record: Arc::new(record),
            manifest: addon.manifest.clone(),
        });
    }
    app.input(Input::Accept);
    wait(&mut app, |app| app.sources_loading == 0);
    assert_eq!(app.sources.len(), 80);
    assert!(app.take_playback().is_none());
}

#[test]
fn a_late_catalog_reply_cannot_restore_rows_from_an_old_profile() {
    let directory = FixtureDirectory::new();
    let original = fixtures::transport();
    let block_once = AtomicBool::new(true);
    let (started_send, started) = mpsc::channel();
    let (release, released) = mpsc::channel();
    let released = Mutex::new(released);
    let transport = fake(move |request| {
        if request.url.path().contains("/primary/catalog/movie/")
            && block_once.swap(false, Ordering::AcqRel)
        {
            started_send.send(()).unwrap();
            released
                .lock()
                .unwrap()
                .recv_timeout(Duration::from_secs(3))
                .unwrap();
        }
        original.send(request)
    });
    let (mut app, _) = app(&directory, transport);
    sign_in(&mut app);
    app.select_profile(0);
    wait(&mut app, |app| !app.home.rows.is_empty());
    started.recv_timeout(Duration::from_secs(2)).unwrap();
    app.select_profile(2);
    wait(&mut app, home_ready);
    release.send(()).unwrap();
    for _ in 0..20 {
        app.tick();
        std::thread::sleep(Duration::from_millis(1));
    }
    assert!(app
        .home
        .rows
        .iter()
        .all(|row| row.name.starts_with("Independent")));
    assert!(app
        .home
        .rows
        .iter()
        .flat_map(|row| &row.titles)
        .all(|title| title
            .id
            .trim_start_matches("ttfixture")
            .parse::<usize>()
            .unwrap()
            >= 10));
}

#[test]
fn a_cancelled_login_cannot_replace_a_newer_login() {
    let directory = FixtureDirectory::new();
    let original = fixtures::transport();
    let first = AtomicBool::new(true);
    let (started_send, started) = mpsc::channel();
    let (release, released) = mpsc::channel();
    let released = Mutex::new(released);
    let transport = fake(move |request| {
        if request.url.path() == "/auth/v1/token" {
            if first.swap(false, Ordering::AcqRel) {
                started_send.send(()).unwrap();
                released
                    .lock()
                    .unwrap()
                    .recv_timeout(Duration::from_secs(3))
                    .unwrap();
                return Ok(response(200, tokens("old-access", "old-refresh")));
            }
            return Ok(response(200, tokens("new-access", "new-refresh")));
        }
        original.send(request)
    });
    let (mut app, vault) = app(&directory, transport);
    app.submit_password("first@example.invalid", Secret::new("first"));
    started.recv_timeout(Duration::from_secs(2)).unwrap();
    app.input(Input::Back);
    app.submit_password("second@example.invalid", Secret::new("second"));
    wait(&mut app, |app| app.page == Page::Profiles);
    release.send(()).unwrap();
    for _ in 0..20 {
        app.tick();
        std::thread::sleep(Duration::from_millis(1));
    }
    assert_eq!(
        vault.snapshot().unwrap().1.access_token.expose(),
        "new-access"
    );
}

#[test]
fn physical_text_entry_preserves_password_characters_and_deletes_a_character() {
    let directory = FixtureDirectory::new();
    let (mut app, _) = app(&directory, fixtures::transport());
    app.input(Input::Accept);
    app.text_input("user+tag@example.invalid");
    app.input(Input::Confirm);
    assert_eq!(app.email, "user+tag@example.invalid");
    app.input(Input::Down);
    app.input(Input::Accept);
    app.text_input("é🙂");
    app.input(Input::Delete);
    app.input(Input::Confirm);
    assert_eq!(app.password.expose(), "é");
}
