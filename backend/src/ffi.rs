//! Native UI boundary: opaque account handles, owned snapshots and actions.
//! No Rust collection layout, allocator ownership or secret is borrowed by C.
use crate::{
    account::{AccountBackend, AccountSnapshot, LoginState, SystemClock},
    config::AccountConfig,
    controller::AccountController,
    error::{Error, Result},
    http::{HttpTransport, Response},
    model::{ProfileId, ProfileLock, Secret},
    service::AccountService,
    storage::AccountStore,
};
use std::{
    ffi::{c_char, CStr, CString},
    ptr,
    sync::Arc,
};

#[repr(C)]
pub struct ProfileView {
    pub id: u32,
    pub name: *const c_char,
    pub color: *const c_char,
    pub avatar: *const c_char,
    pub background: *const c_char,
    pub primary: i32,
    pub lock: i32,
    pub share_primary_addons: i32,
}
#[repr(C)]
pub struct SnapshotView {
    pub revision: u64,
    pub login: i32,
    pub user: *const c_char,
    pub code: *const c_char,
    pub login_url: *const c_char,
    pub error: *const c_char,
    pub email_error: *const c_char,
    pub owner: *const c_char,
    pub persistence_error: i32,
    pub profiles: *const ProfileView,
    pub profile_count: usize,
    pub selected: u32,
    pub needs_selection: i32,
    pub can_dismiss: i32,
    pub suggested_slot: usize,
    pub selection_action: u64,
    pub selection_result: i32,
}
pub struct NativeSnapshot {
    view: SnapshotView,
    _profiles: Box<[ProfileView]>,
    _strings: Vec<CString>,
}
impl NativeSnapshot {
    pub fn new(snapshot: AccountSnapshot) -> Self {
        let mut strings = Vec::new();
        let mut string = |value: &str| {
            // Embedded NUL is valid JSON; display it as a space in the C UI.
            strings.push(CString::new(value.replace('\0', " ")).expect("sanitized native text"));
            strings.last().expect("native text").as_ptr()
        };
        let user = string(&snapshot.user);
        let code = string(&snapshot.code);
        let login_url = string(&snapshot.login_url);
        let error = string(
            &snapshot
                .error
                .map(|error| error.to_string())
                .unwrap_or_default(),
        );
        let email_error = string(
            &snapshot
                .email_error
                .map(|error| error.to_string())
                .unwrap_or_default(),
        );
        let owner = string(snapshot.owner.as_deref().unwrap_or_default());
        let profiles: Box<_> = snapshot
            .selection
            .profiles
            .iter()
            .map(|profile| ProfileView {
                id: profile.id.get(),
                name: string(&profile.name),
                color: string(&profile.color),
                avatar: string(&profile.avatar),
                background: string(&profile.background),
                primary: i32::from(profile.primary),
                lock: match profile.lock {
                    ProfileLock::Unknown => 0,
                    ProfileLock::Unlocked => 1,
                    ProfileLock::Locked => 2,
                },
                share_primary_addons: i32::from(profile.share_primary_addons),
            })
            .collect::<Vec<_>>()
            .into_boxed_slice();
        let view = SnapshotView {
            revision: snapshot.revision,
            login: match snapshot.login {
                LoginState::LoggedOut => 0,
                LoginState::RequestingCode => 1,
                LoginState::Waiting => 2,
                LoginState::Exchanging => 3,
                LoginState::LoggedIn => 4,
                LoginState::Error => 5,
                LoginState::Email => 6,
            },
            user,
            code,
            login_url,
            error,
            email_error,
            owner,
            persistence_error: i32::from(snapshot.persistence_error),
            profiles: profiles.as_ptr(),
            profile_count: profiles.len(),
            selected: snapshot.selection.selected.map(ProfileId::get).unwrap_or(1),
            needs_selection: i32::from(
                snapshot.selection.needs_selection()
                    || (!snapshot.user.is_empty() && snapshot.selection.profiles.is_empty()),
            ),
            can_dismiss: i32::from(snapshot.selection.can_dismiss()),
            suggested_slot: snapshot.selection.suggested_slot(),
            selection_action: snapshot.selection_action,
            selection_result: match snapshot.selection_result {
                None => 0,
                Some(Ok(())) => 1,
                Some(Err(Error::WrongPin)) => -1,
                Some(Err(Error::Cancelled)) => -3,
                Some(Err(_)) => -2,
            },
        };
        Self {
            view,
            _profiles: profiles,
            _strings: strings,
        }
    }
}

unsafe fn text<'a>(value: *const c_char) -> Result<&'a str> {
    if value.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
unsafe fn account<'a>(handle: *const AccountController) -> Option<&'a AccountController> {
    unsafe { handle.as_ref() }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_user(handle: *const AccountController) -> *mut c_char {
    let user = unsafe { account(handle) }
        .map(|account| account.backend.user_id())
        .unwrap_or_default();
    CString::new(user.replace('\0', " "))
        .expect("sanitized identity")
        .into_raw()
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_text_destroy(text: *mut c_char) {
    if !text.is_null() {
        drop(unsafe { CString::from_raw(text) });
    }
}

#[no_mangle]
pub unsafe extern "C" fn nuvio_account_configuration_matches(
    base: *const c_char,
    public_key: *const c_char,
    login_base: *const c_char,
) -> i32 {
    match (unsafe { text(base) }, unsafe { text(public_key) }, unsafe {
        text(login_base)
    }) {
        (Ok(base), Ok(key), Ok(login)) => {
            i32::from(AccountConfig::matches_compiled(base, key, login))
        }
        _ => 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn nuvio_account_create(
    directory: *const c_char,
    app_id: *const c_char,
) -> *mut AccountController {
    let result = (|| {
        let config = AccountConfig::built()?;
        let id = unsafe { text(app_id)? };
        if id != crate::TEST_APP_ID {
            return Err(Error::ProductionData);
        }
        let directory = unsafe { text(directory)? };
        let store = if directory.is_empty() {
            None
        } else {
            Some(AccountStore::open(directory, id)?)
        };
        let service = AccountService::new(config, Arc::new(HttpTransport::default()));
        AccountController::new(AccountBackend::new(service, store, Arc::new(SystemClock)))
    })();
    result
        .map(|controller| Box::into_raw(Box::new(controller)))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_destroy(handle: *mut AccountController) {
    if !handle.is_null() {
        drop(unsafe { Box::from_raw(handle) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_snapshot(
    handle: *const AccountController,
) -> *mut NativeSnapshot {
    unsafe { account(handle) }
        .map(|account| Box::into_raw(Box::new(NativeSnapshot::new(account.backend.snapshot()))))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_snapshot_view(
    snapshot: *const NativeSnapshot,
) -> *const SnapshotView {
    unsafe { snapshot.as_ref() }
        .map(|snapshot| &snapshot.view as *const _)
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_snapshot_destroy(snapshot: *mut NativeSnapshot) {
    if !snapshot.is_null() {
        drop(unsafe { Box::from_raw(snapshot) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_step(handle: *const AccountController) {
    if let Some(account) = unsafe { account(handle) } {
        account.step();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_revision(handle: *const AccountController) -> u64 {
    unsafe { account(handle) }
        .map(|account| account.backend.revision())
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_active_id(handle: *const AccountController) -> u32 {
    unsafe { account(handle) }
        .map(|account| account.backend.active_id())
        .unwrap_or(1)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_logged_in(handle: *const AccountController) -> i32 {
    i32::from(unsafe { account(handle) }.is_some_and(|account| account.backend.logged_in()))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_needs_selection(handle: *const AccountController) -> i32 {
    i32::from(unsafe { account(handle) }.is_some_and(|account| account.backend.needs_selection()))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_login_email(
    handle: *const AccountController,
    email: *const c_char,
    password: *const c_char,
) {
    if let (Some(account), Ok(email), Ok(password)) =
        (unsafe { account(handle) }, unsafe { text(email) }, unsafe {
            text(password)
        })
    {
        account.email_login(Secret::new(email), Secret::new(password));
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_login_tv(handle: *const AccountController) {
    if let Some(account) = unsafe { account(handle) } {
        account.tv_login();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_cancel(handle: *const AccountController) {
    if let Some(account) = unsafe { account(handle) } {
        account.cancel();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_logout(handle: *const AccountController) {
    if let Some(account) = unsafe { account(handle) } {
        account.logout();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_select(
    handle: *const AccountController,
    id: u32,
    pin: *const c_char,
) -> u64 {
    match (unsafe { account(handle) }, ProfileId::new(id)) {
        (Some(account), Some(id)) => account.select(
            id,
            if pin.is_null() {
                None
            } else {
                unsafe { text(pin) }.ok().map(Secret::new)
            },
        ),
        _ => 0,
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_cancel_selection(handle: *const AccountController) {
    if let Some(account) = unsafe { account(handle) } {
        account.cancel_selection();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_keep_profile(handle: *const AccountController) -> i32 {
    i32::from(
        unsafe { account(handle) }.is_some_and(|account| account.backend.keep_profile().is_ok()),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_load_profiles(handle: *const AccountController) -> i32 {
    unsafe { account(handle) }
        .and_then(|account| account.backend.load_profiles().ok())
        .map(|count| count as i32)
        .unwrap_or(-1)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_addon_profile(
    handle: *const AccountController,
    force_primary: i32,
) -> u32 {
    unsafe { account(handle) }
        .and_then(|account| {
            account
                .backend
                .snapshot()
                .selection
                .addon_profile(force_primary != 0)
        })
        .map(ProfileId::get)
        .unwrap_or(0)
}

/// Transitional RPC/table results are owned allocations with one release API.
/// C adapters copy them into C allocations for still-unmigrated C consumers.
#[repr(C)]
pub struct ReplyView {
    pub status: i32,
    pub data: *const u8,
    pub length: usize,
}
pub struct NativeReply {
    view: ReplyView,
    _response: Option<Response>,
}
impl NativeReply {
    fn new(result: Result<Response>) -> Self {
        match result {
            Ok(response) => Self {
                view: ReplyView {
                    status: i32::from(response.status),
                    data: response.body.as_ptr(),
                    length: response.body.len(),
                },
                _response: Some(response),
            },
            Err(error) => Self {
                view: ReplyView {
                    status: match error {
                        Error::Http(status) => i32::from(status),
                        Error::InvalidSession | Error::LoggedOut => 401,
                        _ => 0,
                    },
                    data: ptr::null(),
                    length: 0,
                },
                _response: None,
            },
        }
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_request(
    handle: *const AccountController,
    table: i32,
    name: *const c_char,
    argument: *const c_char,
) -> *mut NativeReply {
    let result = (|| {
        let account = unsafe { account(handle) }.ok_or(Error::Configuration)?;
        let name = unsafe { text(name)? };
        let argument = unsafe { text(argument)? };
        if table != 0 {
            account.backend.table(name, argument)
        } else {
            account.backend.rpc(name, argument.as_bytes())
        }
    })();
    Box::into_raw(Box::new(NativeReply::new(result)))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_reply_view(reply: *const NativeReply) -> *const ReplyView {
    unsafe { reply.as_ref() }
        .map(|reply| &reply.view as *const _)
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_account_reply_destroy(reply: *mut NativeReply) {
    if !reply.is_null() {
        drop(unsafe { Box::from_raw(reply) });
    }
}

#[no_mangle]
pub unsafe extern "C" fn nuvio_account_copy_token(
    handle: *const AccountController,
    output: *mut c_char,
    capacity: usize,
) -> i32 {
    if output.is_null() || capacity == 0 {
        return 0;
    }
    unsafe {
        *output = 0;
    }
    let Some(token) = (unsafe { account(handle) }).and_then(|account| account.backend.user_token())
    else {
        return 0;
    };
    if token.expose().len() >= capacity {
        return 0;
    }
    unsafe {
        ptr::copy_nonoverlapping(token.expose().as_ptr(), output.cast(), token.expose().len());
        *output.add(token.expose().len()) = 0;
    }
    1
}
