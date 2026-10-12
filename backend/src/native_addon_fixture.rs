//! Test-only account transport for retained C source/subtitle algorithm checks.
//! Uses the real addon model, backend, journal, controller and native snapshots.
//! Excluded from the normal Linux and TV builds.
use crate::{
    addon_backend::AddonBackend,
    addon_controller::AddonController,
    addon_ffi::NativeAddonController,
    addon_service::AddonAccount,
    addon_storage::AddonStore,
    addons::InstalledAddon,
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
    model::ProfileId,
};
use std::{
    ffi::{c_char, CStr},
    ptr,
    sync::{Arc, Mutex},
};

struct State {
    context: ContentContext,
    addons: Vec<InstalledAddon>,
}
struct Account(Mutex<State>);
impl AddonAccount for Account {
    fn current_context(&self, _: bool) -> Option<ContentContext> {
        Some(self.0.lock().unwrap().context.clone())
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.0.lock().unwrap().context == *context
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        let state = self.0.lock().unwrap();
        if state.context != *context {
            return Err(Error::Cancelled);
        }
        Ok(state.addons.clone())
    }
    fn write_addons(&self, context: &ContentContext, addons: &[InstalledAddon]) -> Result<()> {
        let mut state = self.0.lock().unwrap();
        if state.context != *context {
            return Err(Error::Cancelled);
        }
        state.addons = addons.to_vec();
        Ok(())
    }
}
pub struct Fixture {
    account: Arc<Account>,
    native: NativeAddonController,
}
unsafe fn text<'a>(value: *const c_char) -> Result<&'a str> {
    if value.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_test_addon_fixture_create(directory: *const c_char) -> *mut Fixture {
    let result = (|| {
        let profile = ProfileId::new(1).ok_or(Error::InvalidData)?;
        let account = Arc::new(Account(Mutex::new(State {
            context: ContentContext {
                account_generation: 1,
                content_generation: 1,
                profile,
                force_primary: false,
                addons: AddonScope {
                    member: "fixture-member".into(),
                    owner: "fixture-owner".into(),
                    profile,
                },
            },
            addons: Vec::new(),
        })));
        let backend = AddonBackend::new(
            account.clone(),
            Arc::new(AddonStore::open(unsafe { text(directory)? })?),
        );
        let native = NativeAddonController::new(AddonController::new(backend)?);
        Ok::<_, Error>(Fixture { account, native })
    })();
    result
        .map(|fixture| Box::into_raw(Box::new(fixture)))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_test_addon_fixture_handle(
    fixture: *mut Fixture,
) -> *mut NativeAddonController {
    unsafe { fixture.as_mut() }
        .map(|fixture| &mut fixture.native as *mut _)
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_test_addon_fixture_replace(
    fixture: *const Fixture,
    body: *const c_char,
) -> i32 {
    let result = (|| {
        let fixture = unsafe { fixture.as_ref() }.ok_or(Error::InvalidData)?;
        let addons = InstalledAddon::decode_account_list(unsafe { text(body)? }.as_bytes())?;
        {
            let mut state = fixture.account.0.lock().unwrap();
            state.context.content_generation += 1;
            state.addons = addons;
        }
        let backend = &fixture.native.controller.backend;
        let context = backend.activate(false).ok_or(Error::Cancelled)?;
        backend.synchronize(&context)
    })();
    i32::from(result.is_ok())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_test_addon_fixture_destroy(fixture: *mut Fixture) {
    if !fixture.is_null() {
        drop(unsafe { Box::from_raw(fixture) });
    }
}
