#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    addon_backend::{AddonBackend, AddonStatus},
    addon_controller::AddonController,
    addon_ffi::{nuvio_addons_destroy, NativeAddonController},
    addon_service::AddonAccount,
    addon_snapshot::{nuvio_addons_snapshot_destroy, NativeAddonSnapshot},
    addon_storage::AddonStore,
    addons::{AddonUrl, InstalledAddon, Manifest},
    content_context::{AddonScope, ContentContext},
    error::Result,
    model::ProfileId,
};
use std::{
    collections::HashMap,
    ffi::{c_char, c_void, CStr, CString},
    io::{Read, Write},
    net::TcpListener,
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        Arc, Condvar, Mutex,
    },
    thread,
    time::{Duration, Instant},
};
use support::Directory;

#[link(name = "nuvio_c_ui")]
extern "C" {
    fn addon_registry_test_attach(controller: *mut c_void);
    fn addon_registry_refresh();
    fn addon_registry_use(snapshot: *const NativeAddonSnapshot);
    fn addon_registry_reader_current() -> i32;
    fn addon_registry_step(primary: i32, periodic: i32);
    fn addons_n() -> i32;
    fn addons_base(index: i32) -> *const c_char;
    fn addons_nome(index: i32) -> *const c_char;
    fn addons_alternar(index: i32) -> i32;
    fn addons_buscar_legendas(id: *const c_char, kind: *const c_char);
    fn addons_legendas_prontas() -> i32;
    fn addons_n_legendas() -> i32;
    fn addons_encerrar();
    fn native_addons_test_source_count(
        id: *const c_char,
        kind: *const c_char,
        base: *const c_char,
    ) -> i32;
}
#[link(name = "SDL2")]
extern "C" {
    fn SDL_Init(flags: u32) -> i32;
    fn SDL_Quit();
}

fn wait(mut condition: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(8);
    while !condition() {
        assert!(
            Instant::now() < deadline,
            "native addon operation timed out"
        );
        thread::sleep(Duration::from_millis(2));
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
            profile: ProfileId::new(profile).unwrap(),
        },
    }
}
struct Account {
    selected: Mutex<ContentContext>,
    lists: Mutex<HashMap<u32, Vec<InstalledAddon>>>,
}
impl AddonAccount for Account {
    fn current_context(&self, _: bool) -> Option<ContentContext> {
        Some(self.selected.lock().unwrap().clone())
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        &*self.selected.lock().unwrap() == context
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        Ok(self
            .lists
            .lock()
            .unwrap()
            .get(&context.addons.profile.get())
            .cloned()
            .unwrap_or_default())
    }
    fn write_addons(&self, context: &ContentContext, addons: &[InstalledAddon]) -> Result<()> {
        self.lists
            .lock()
            .unwrap()
            .insert(context.addons.profile.get(), addons.to_vec());
        Ok(())
    }
}
struct Fixture {
    account: Arc<Account>,
    backend: AddonBackend,
    handle: *mut NativeAddonController,
    _directory: Directory,
}
impl Fixture {
    fn new(addons: Vec<InstalledAddon>) -> Self {
        let directory = Directory::new();
        let account = Arc::new(Account {
            selected: Mutex::new(context(1, 1)),
            lists: Mutex::new(HashMap::from([(1, addons)])),
        });
        let backend = AddonBackend::new(
            account.clone(),
            Arc::new(AddonStore::open(&directory.path).unwrap()),
        );
        let handle = Box::into_raw(Box::new(NativeAddonController::new(
            AddonController::new(backend.clone()).unwrap(),
        )));
        unsafe {
            addon_registry_test_attach(handle.cast());
            addon_registry_step(0, 0);
        }
        wait(|| backend.snapshot().status == AddonStatus::Ready);
        unsafe {
            addon_registry_refresh();
        }
        Self {
            account,
            backend,
            handle,
            _directory: directory,
        }
    }
}
impl Drop for Fixture {
    fn drop(&mut self) {
        unsafe {
            addons_encerrar();
            addon_registry_test_attach(std::ptr::null_mut());
            nuvio_addons_destroy(self.handle);
        }
    }
}
#[derive(Default)]
struct Gate {
    state: Mutex<(bool, bool)>,
    ready: Condvar,
}
impl Gate {
    fn hold(&self) {
        let mut state = self.state.lock().unwrap();
        state.0 = true;
        self.ready.notify_all();
        let deadline = Instant::now() + Duration::from_secs(8);
        while !state.1 && Instant::now() < deadline {
            state = self
                .ready
                .wait_timeout(state, Duration::from_millis(20))
                .unwrap()
                .0;
        }
    }
    fn release(&self) {
        self.state.lock().unwrap().1 = true;
        self.ready.notify_all();
    }
}
struct Server {
    base: String,
    requests: Arc<Mutex<Vec<String>>>,
    gate: Option<Arc<Gate>>,
    stop: Arc<AtomicBool>,
    thread: Option<thread::JoinHandle<()>>,
}
impl Server {
    fn new(gate: Option<Arc<Gate>>) -> Self {
        Self::with_initial_failure(gate, false)
    }
    fn with_initial_failure(gate: Option<Arc<Gate>>, fail_first: bool) -> Self {
        let listener = TcpListener::bind("127.0.0.1:0").unwrap();
        let base = format!("http://{}", listener.local_addr().unwrap());
        listener.set_nonblocking(true).unwrap();
        let requests = Arc::new(Mutex::new(Vec::new()));
        let log = requests.clone();
        let stop = Arc::new(AtomicBool::new(false));
        let finished = stop.clone();
        let held = gate.clone();
        let sequence = Arc::new(AtomicUsize::new(0));
        let thread = thread::spawn(move || {
            let mut workers = Vec::new();
            while !finished.load(Ordering::SeqCst) {
                match listener.accept() {
                    Ok((mut stream, _)) => {
                        let log = log.clone();
                        let held = held.clone();
                        let sequence = sequence.clone();
                        workers.push(thread::spawn(move || {
                            stream.set_read_timeout(Some(Duration::from_secs(5))).unwrap();
                            let mut request = Vec::new(); let mut buffer = [0; 2048];
                            while !request.windows(4).any(|part| part == b"\r\n\r\n") {
                                match stream.read(&mut buffer) { Ok(0) | Err(_) => return, Ok(count) => request.extend_from_slice(&buffer[..count]) }
                                if request.len() > 128 * 1024 { return; }
                            }
                            log.lock().unwrap().push(String::from_utf8(request).unwrap());
                            if let Some(gate) = held { gate.hold(); }
                            let status = if fail_first && sequence.fetch_add(1, Ordering::SeqCst) == 0 { "500 Internal Server Error" } else { "200 OK" };
                            let body = br#"{"streams":[{"name":"Fixture","title":"Source","url":"https://video.invalid/movie.mkv"}],"subtitles":[{"lang":"en","url":"https://subtitles.invalid/movie.srt","subtitleFileName":"Movie.WEB.srt"}]}"#;
                            let header = format!("HTTP/1.1 {status}\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: {}\r\n\r\n", body.len());
                            let _ = stream.write_all(header.as_bytes()); let _ = stream.write_all(body);
                        }));
                    }
                    Err(error) if error.kind() == std::io::ErrorKind::WouldBlock => {
                        thread::sleep(Duration::from_millis(2))
                    }
                    Err(_) => break,
                }
            }
            for worker in workers {
                worker.join().unwrap();
            }
        });
        Self {
            base,
            requests,
            gate,
            stop,
            thread: Some(thread),
        }
    }
}
impl Drop for Server {
    fn drop(&mut self) {
        if let Some(gate) = &self.gate {
            gate.release();
        }
        self.stop.store(true, Ordering::SeqCst);
        self.thread.take().unwrap().join().unwrap();
    }
}
fn addon(url: &str, index: usize) -> InstalledAddon {
    InstalledAddon {
        name: format!("Provider {index}"),
        url: AddonUrl::parse(url).unwrap(),
        enabled: true,
    }
}

#[test]
fn native_readers_and_source_pool_use_every_provider_with_complete_encoded_urls() {
    let server = Server::new(None);
    let config = "configuration".repeat(600);
    let providers: Vec<_> = (0..80)
        .map(|index| {
            addon(
                &format!("{}/{config}/provider{index}?key=retained", server.base),
                index,
            )
        })
        .collect();
    let expected_url = providers[79].url.clone();
    let fixture = Fixture::new(providers);
    let id = CString::new(format!("kitsu:{} / 日?#%", "123456789".repeat(180))).unwrap();
    let kind = CString::new("series").unwrap();
    unsafe {
        assert_eq!(SDL_Init(1), 0);
        assert_eq!(addons_n(), 80);
        assert_eq!(
            CStr::from_ptr(addons_base(79)).to_str().unwrap(),
            expected_url.as_str()
        );
        assert_eq!(
            native_addons_test_source_count(id.as_ptr(), kind.as_ptr(), std::ptr::null()),
            80
        );
        SDL_Quit();
    }
    let requests = server.requests.lock().unwrap();
    assert_eq!(requests.len(), 80);
    let target = expected_url
        .resource_url("stream", "series", id.to_str().unwrap())
        .unwrap();
    let path = format!("GET {}?{} HTTP/1.1", target.path(), target.query().unwrap());
    assert!(requests.iter().any(|request| request.starts_with(&path)));
    for request in requests.iter() {
        let lower = request.to_ascii_lowercase();
        assert!(!lower.contains("\r\nauthorization:"));
        assert!(!lower.contains("\r\napikey:"));
        assert!(!lower.contains("%2520"));
    }
    drop(requests);
    assert_eq!(fixture.backend.snapshot().addons.records.len(), 80);
}

#[test]
fn source_retry_keeps_the_originating_rust_snapshot_and_publishes_recovered_sources() {
    let server = Server::with_initial_failure(None, true);
    let _fixture = Fixture::new(vec![addon(&format!("{}/retry", server.base), 0)]);
    let id = CString::new("tt789").unwrap();
    let kind = CString::new("movie").unwrap();
    assert_eq!(
        unsafe { native_addons_test_source_count(id.as_ptr(), kind.as_ptr(), std::ptr::null()) },
        1
    );
    assert_eq!(server.requests.lock().unwrap().len(), 2);
}

#[test]
fn profile_changes_keep_pinned_readers_alive_and_cancel_late_native_sources() {
    let gate = Arc::new(Gate::default());
    let server = Server::new(Some(gate.clone()));
    let old = addon(&format!("{}/old?key=first", server.base), 0);
    let fixture = Fixture::new(vec![old]);
    let held = Box::into_raw(Box::new(unsafe { (*fixture.handle).snapshot() })) as usize;
    let finished = Arc::new(Gate::default());
    let signal = finished.clone();
    let reader = thread::spawn(move || unsafe {
        addon_registry_use(held as *const NativeAddonSnapshot);
        let name = addons_nome(0);
        signal.hold();
        assert_eq!(CStr::from_ptr(name).to_str().unwrap(), "Provider 0");
        assert_eq!(addon_registry_reader_current(), 0);
    });
    wait(|| finished.state.lock().unwrap().0);
    let source = thread::spawn(move || unsafe {
        let id = CString::new("tt123").unwrap();
        let kind = CString::new("movie").unwrap();
        native_addons_test_source_count(id.as_ptr(), kind.as_ptr(), std::ptr::null())
    });
    wait(|| gate.state.lock().unwrap().0);
    *fixture.account.selected.lock().unwrap() = context(2, 2);
    unsafe {
        addon_registry_step(0, 0);
    }
    assert_eq!(unsafe { addons_n() }, 0);
    gate.release();
    finished.release();
    assert_eq!(source.join().unwrap(), -1);
    reader.join().unwrap();
    unsafe {
        nuvio_addons_snapshot_destroy(held as *mut NativeAddonSnapshot);
    }
    assert_eq!(server.requests.lock().unwrap().len(), 1);
    assert!(fixture.backend.snapshot().addons.records.is_empty());
}

#[test]
fn subtitles_visit_every_provider_with_complete_urls_and_only_four_in_flight() {
    let gate = Arc::new(Gate::default());
    let server = Server::new(Some(gate.clone()));
    let configuration = "configured".repeat(700);
    let providers: Vec<_> = (0..80)
        .map(|index| {
            addon(
                &format!(
                    "{}/{configuration}/provider{index}?key=retained",
                    server.base
                ),
                index,
            )
        })
        .collect();
    let last_url = providers[79].url.clone();
    let _fixture = Fixture::new(providers);
    let id = CString::new("tt123").unwrap();
    let kind = CString::new("movie").unwrap();
    unsafe {
        addons_buscar_legendas(id.as_ptr(), kind.as_ptr());
    }
    wait(|| server.requests.lock().unwrap().len() == 4);
    // Holding all four responses must keep provider five queued. The queue is
    // exercised with eighty providers, independently of the twelve visible rows.
    thread::sleep(Duration::from_millis(80));
    assert_eq!(server.requests.lock().unwrap().len(), 4);
    assert_eq!(unsafe { addons_legendas_prontas() }, 0);
    gate.release();
    wait(|| unsafe { addons_legendas_prontas() } == 1);
    assert_eq!(unsafe { addons_n_legendas() }, 12);
    let requests = server.requests.lock().unwrap();
    assert_eq!(requests.len(), 80);
    let target = last_url
        .resource_url("subtitles", "movie", "tt123")
        .unwrap();
    let path = format!("GET {}?{} HTTP/1.1", target.path(), target.query().unwrap());
    assert!(requests.iter().any(|request| request.starts_with(&path)));
    assert!(requests
        .iter()
        .all(|request| !request.to_ascii_lowercase().contains("\r\nauthorization:")));
}

#[test]
fn profile_switch_cancels_queued_subtitles_and_hides_old_ready_results() {
    let gate = Arc::new(Gate::default());
    let server = Server::new(Some(gate.clone()));
    let providers = (0..12)
        .map(|index| addon(&format!("{}/provider{index}", server.base), index))
        .collect();
    let fixture = Fixture::new(providers);
    let id = CString::new("tt456").unwrap();
    let kind = CString::new("movie").unwrap();
    unsafe {
        addons_buscar_legendas(id.as_ptr(), kind.as_ptr());
    }
    wait(|| server.requests.lock().unwrap().len() == 4);
    *fixture.account.selected.lock().unwrap() = context(2, 2);
    unsafe {
        addon_registry_step(0, 0);
    }
    assert_eq!(unsafe { addons_n_legendas() }, 0);
    assert_eq!(unsafe { addons_legendas_prontas() }, 0);
    gate.release();
    // Shutdown joins the actual workers; no timing assumption decides whether
    // a late response was published or another queued provider was requested.
    unsafe {
        addons_encerrar();
    }
    assert_eq!(server.requests.lock().unwrap().len(), 4);
    assert_eq!(unsafe { addons_n_legendas() }, 0);
    assert_eq!(unsafe { addons_legendas_prontas() }, 0);
}
