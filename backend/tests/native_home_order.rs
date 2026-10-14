#![cfg(feature = "native-ui-tests")]
mod support;
use nuvio_backend::{
    catalog_order::{HomeOrder, HomeOrderStorage, HomeOrderStore},
    catalog_order_ffi::{
        nuvio_home_order_destroy, nuvio_home_order_snapshot, nuvio_home_order_snapshot_destroy,
        nuvio_home_order_snapshot_key, NativeHomeOrderController,
    },
    content_context::{AddonScope, ContentContext},
    error::Result,
    home_order_backend::{HomeOrderAccount, HomeOrderBackend},
    home_order_controller::HomeOrderController,
    model::ProfileId,
};
use std::{
    ffi::{c_char, c_void, CStr},
    sync::{Arc, Barrier, Mutex},
    thread,
    time::{Duration, Instant},
};
use support::Directory;
#[link(name = "nuvio_c_ui")]
extern "C" {
    fn home_order_runtime_test_attach(controller: *mut c_void);
    fn home_order_runtime_step(allow_periodic: i32);
    fn home_order_runtime_refresh();
    fn home_order_runtime_logout();
    fn catordem_revisao() -> u32;
    fn catordem_n() -> i32;
    fn catordem_chave(index: i32) -> *const c_char;
    fn catordem_oculta(key: *const c_char, provider: *const c_char) -> i32;
    fn catordem_tem_ocultar_nao_lancados() -> i32;
    fn catordem_ocultar_nao_lancados() -> i32;
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
struct Gate {
    started: Barrier,
    release: Barrier,
}
struct Account {
    context: Mutex<Option<ContentContext>>,
    gate: Mutex<Option<Arc<Gate>>>,
    key: String,
}
impl HomeOrderAccount for Account {
    fn current_context(&self) -> Option<ContentContext> {
        self.context.lock().unwrap().clone()
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.current_context().as_ref() == Some(context)
    }
    fn read_order(&self, context: &ContentContext) -> Result<Option<HomeOrder>> {
        let gate = self.gate.lock().unwrap().take();
        if let Some(gate) = gate {
            gate.started.wait();
            gate.release.wait();
        }
        Ok(Some(if context.profile.get() == 2 {
            cached(&self.key)
        } else {
            HomeOrder {
                keys: vec!["third-profile".into()],
                ..Default::default()
            }
        }))
    }
}
fn cached(key: &str) -> HomeOrder {
    HomeOrder {
        keys: vec![key.into()],
        hidden: [key.into()].into(),
        hide_unreleased: Some(true),
        hide_underline: Some(false),
    }
}
fn wait_for(mut ready: impl FnMut() -> bool) {
    let deadline = Instant::now() + Duration::from_secs(5);
    while !ready() {
        assert!(Instant::now() < deadline, "native Home order timed out");
        thread::sleep(Duration::from_millis(2));
    }
}

#[test]
fn real_c_bridge_publishes_cache_without_waiting_for_network_and_preserves_profile_ownership() {
    let directory = Directory::new();
    let ctx = context(2, 1);
    let key = "complete-private-key-".repeat(400);
    let store = Arc::new(HomeOrderStore::open(&directory.path).unwrap());
    store.save(&(&ctx).into(), &cached(&key), &|| true).unwrap();
    let gate = Arc::new(Gate {
        started: Barrier::new(2),
        release: Barrier::new(2),
    });
    let account = Arc::new(Account {
        context: Mutex::new(Some(ctx.clone())),
        gate: Mutex::new(Some(gate.clone())),
        key: key.clone(),
    });
    let backend = HomeOrderBackend::new(account.clone(), store.clone());
    let native = Box::into_raw(Box::new(NativeHomeOrderController::new(
        HomeOrderController::new(backend.clone()).unwrap(),
    )));
    unsafe {
        home_order_runtime_test_attach(native.cast());
        home_order_runtime_step(0);
    }
    gate.started.wait();
    unsafe {
        home_order_runtime_step(0);
        assert_eq!(catordem_n(), 1);
        assert_eq!(CStr::from_ptr(catordem_chave(0)).to_str().unwrap(), key);
        assert_eq!(catordem_oculta(catordem_chave(0), c"".as_ptr()), 1);
        assert_eq!(catordem_ocultar_nao_lancados(), 1);
    }
    let held = nuvio_home_order_snapshot();
    let revision = unsafe { catordem_revisao() };
    gate.release.wait();
    wait_for(|| backend.snapshot().revision >= 3);
    unsafe {
        home_order_runtime_step(0);
        assert_eq!(catordem_revisao(), revision);
    }
    // The same response needs no extra C row rebuild. Hold a subsequent refresh
    // while selecting a new profile, then hold the new profile's network reply.
    let refresh_gate = Arc::new(Gate {
        started: Barrier::new(2),
        release: Barrier::new(2),
    });
    *account.gate.lock().unwrap() = Some(refresh_gate.clone());
    unsafe {
        home_order_runtime_refresh();
    }
    refresh_gate.started.wait();
    let third_gate = Arc::new(Gate {
        started: Barrier::new(2),
        release: Barrier::new(2),
    });
    *account.gate.lock().unwrap() = Some(third_gate.clone());
    *account.context.lock().unwrap() = Some(context(3, 2));
    unsafe {
        home_order_runtime_step(0);
        assert_eq!(catordem_n(), 0);
        assert_eq!(catordem_tem_ocultar_nao_lancados(), 0);
    }
    refresh_gate.release.wait();
    third_gate.started.wait();
    unsafe {
        home_order_runtime_step(0);
        assert_eq!(catordem_n(), 0);
    }
    third_gate.release.wait();
    wait_for(|| backend.snapshot().order.keys == ["third-profile"]);
    unsafe {
        home_order_runtime_step(0);
        assert_eq!(
            CStr::from_ptr(catordem_chave(0)).to_str().unwrap(),
            "third-profile"
        );
        assert_eq!(
            CStr::from_ptr(nuvio_home_order_snapshot_key(held, 0))
                .to_str()
                .unwrap(),
            key
        );
        nuvio_home_order_snapshot_destroy(held);
    }
    *account.context.lock().unwrap() = None;
    unsafe {
        home_order_runtime_logout();
        assert_eq!(catordem_n(), 0);
    }
    wait_for(|| store.load(&(&ctx).into()).unwrap().is_none());
    unsafe {
        home_order_runtime_test_attach(std::ptr::null_mut());
        nuvio_home_order_destroy(native);
    }
}
