use crate::{
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
    http::Response,
    model::{ProfileId, ProfileLock, Secret, Session},
    service::{AccountService, LoginCode, PollResult},
    storage::{AccountData, AccountStore},
};
use std::{
    sync::{Arc, Condvar, Mutex, MutexGuard},
    time::{Instant, SystemTime, UNIX_EPOCH},
};

pub trait Clock: Send + Sync {
    fn seconds(&self) -> u64;
    fn monotonic_millis(&self) -> u64 {
        self.seconds().saturating_mul(1000)
    }
}
pub struct SystemClock;
impl Clock for SystemClock {
    fn seconds(&self) -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs()
    }
    fn monotonic_millis(&self) -> u64 {
        static START: std::sync::OnceLock<Instant> = std::sync::OnceLock::new();
        START
            .get_or_init(Instant::now)
            .elapsed()
            .as_millis()
            .min(u64::MAX as u128) as u64
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum LoginState {
    #[default]
    LoggedOut,
    RequestingCode,
    Waiting,
    Exchanging,
    LoggedIn,
    Error,
    Email,
}

/// An owned, immutable view. It contains no tokens or passwords.
#[derive(Clone, Debug)]
pub struct AccountSnapshot {
    pub revision: u64,
    pub login: LoginState,
    pub user: String,
    pub code: String,
    pub login_url: String,
    pub error: Option<Error>,
    pub email_error: Option<Error>,
    pub persistence_error: bool,
    pub owner: Option<String>,
    pub selection: crate::model::ProfileSelection,
    pub selection_action: u64,
    pub selection_result: Option<Result<()>>,
}

struct State {
    data: AccountData,
    revision: u64,
    generation: u64,
    content_generation: u64,
    login: LoginState,
    code: Option<LoginCode>,
    code_deadline: u64,
    next_poll: u64,
    polling: bool,
    error: Option<Error>,
    email_error: Option<Error>,
    persistence_error: bool,
    refreshing: bool,
    refresh_sequence: u64,
    refresh_result: Result<()>,
    selection_action: u64,
    selection_result: Option<Result<()>>,
}
struct Inner {
    state: Mutex<State>,
    refreshed: Condvar,
    service: AccountService,
    store: Option<AccountStore>,
    clock: Arc<dyn Clock>,
}
#[derive(Clone)]
pub struct AccountBackend {
    inner: Arc<Inner>,
}

impl AccountBackend {
    pub fn new(
        service: AccountService,
        store: Option<AccountStore>,
        clock: Arc<dyn Clock>,
    ) -> Self {
        let (data, persistence_error) = match store.as_ref().map(AccountStore::load) {
            Some(Ok(data)) => (data, false),
            Some(Err(_)) => (AccountData::default(), true),
            None => (AccountData::default(), true),
        };
        let login = if data
            .session
            .as_ref()
            .is_some_and(|session| !session.anonymous)
        {
            LoginState::LoggedIn
        } else {
            LoginState::LoggedOut
        };
        Self {
            inner: Arc::new(Inner {
                state: Mutex::new(State {
                    data,
                    revision: 0,
                    generation: 0,
                    content_generation: 0,
                    login,
                    code: None,
                    code_deadline: 0,
                    next_poll: 0,
                    polling: false,
                    error: None,
                    email_error: None,
                    persistence_error,
                    refreshing: false,
                    refresh_sequence: 0,
                    refresh_result: Ok(()),
                    selection_action: 0,
                    selection_result: None,
                }),
                refreshed: Condvar::new(),
                service,
                store,
                clock,
            }),
        }
    }
    fn state(&self) -> MutexGuard<'_, State> {
        self.inner
            .state
            .lock()
            .unwrap_or_else(|error| error.into_inner())
    }

    pub fn snapshot(&self) -> AccountSnapshot {
        let state = self.state();
        AccountSnapshot {
            revision: state.revision,
            login: state.login,
            user: state
                .data
                .session
                .as_ref()
                .filter(|session| !session.anonymous)
                .map(|session| session.subject.clone())
                .unwrap_or_default(),
            code: state
                .code
                .as_ref()
                .map(|code| code.code.clone())
                .unwrap_or_default(),
            login_url: state
                .code
                .as_ref()
                .map(|code| code.web_url.clone())
                .unwrap_or_default(),
            error: state.error.clone(),
            email_error: state.email_error.clone(),
            persistence_error: state.persistence_error,
            owner: state.data.owner.clone(),
            selection: state.data.selection.clone(),
            selection_action: state.selection_action,
            selection_result: state.selection_result.clone(),
        }
    }
    pub fn revision(&self) -> u64 {
        self.state().revision
    }
    pub fn active_id(&self) -> u32 {
        self.state()
            .data
            .selection
            .selected
            .map(ProfileId::get)
            .unwrap_or(1)
    }
    pub fn user_id(&self) -> String {
        self.state()
            .data
            .session
            .as_ref()
            .filter(|session| !session.anonymous)
            .map(|session| session.subject.clone())
            .unwrap_or_default()
    }
    pub fn logged_in(&self) -> bool {
        self.state()
            .data
            .session
            .as_ref()
            .is_some_and(|session| !session.anonymous)
    }
    pub fn needs_selection(&self) -> bool {
        let state = self.state();
        state.data.selection.needs_selection()
            || (state
                .data
                .session
                .as_ref()
                .is_some_and(|session| !session.anonymous)
                && state.data.selection.profiles.is_empty())
    }

    pub fn user_token(&self) -> Option<Secret> {
        self.state()
            .data
            .session
            .as_ref()
            .filter(|session| !session.anonymous)
            .map(|session| session.access.clone())
    }

    pub fn content_context(&self, force_primary: bool) -> Option<ContentContext> {
        let state = self.state();
        if state.login != LoginState::LoggedIn || state.data.selection.needs_selection() {
            return None;
        }
        let session = state
            .data
            .session
            .as_ref()
            .filter(|session| !session.anonymous)?;
        let owner = state
            .data
            .owner
            .as_ref()
            .filter(|owner| !owner.is_empty())?;
        let profile = state.data.selection.active()?;
        if profile.lock == ProfileLock::Unknown {
            return None;
        }
        Some(ContentContext {
            account_generation: state.generation,
            content_generation: state.content_generation,
            profile: profile.id,
            addons: AddonScope {
                member: session.subject.clone(),
                owner: owner.clone(),
                profile: state.data.selection.addon_profile(force_primary)?,
            },
            force_primary,
        })
    }
    pub fn content_context_current(&self, context: &ContentContext) -> bool {
        self.content_context(context.force_primary).as_ref() == Some(context)
    }

    pub fn begin_email(&self) -> u64 {
        self.begin_flow(LoginState::Email)
    }
    pub fn begin_tv(&self) -> u64 {
        self.begin_flow(LoginState::RequestingCode)
    }
    fn begin_flow(&self, login: LoginState) -> u64 {
        eprintln!(
            "[account] Rust login started: {}",
            if login == LoginState::Email {
                "email"
            } else {
                "TV code"
            }
        );
        let mut state = self.state();
        state.generation = state.generation.wrapping_add(1);
        state.login = login;
        state.code = None;
        state.error = None;
        state.email_error = None;
        state.polling = false;
        state.selection_action = state.selection_action.wrapping_add(1);
        state.selection_result = None;
        state.revision = state.revision.wrapping_add(1);
        state.generation
    }

    pub fn finish_email(&self, generation: u64, email: Secret, password: Secret) -> Result<()> {
        if self.state().generation != generation {
            return Err(Error::Cancelled);
        }
        let result = self.inner.service.password_login(&email, &password);
        let mut state = self.state();
        if state.generation != generation {
            return Err(Error::Cancelled);
        }
        match result {
            Ok(session) => {
                eprintln!("[account] Rust email login completed");
                self.install_session(&mut state, session);
                Ok(())
            }
            Err(error) => {
                eprintln!("[account] Rust email login failed: {error}");
                state.login = LoginState::LoggedOut;
                state.email_error = Some(error.clone());
                state.revision = state.revision.wrapping_add(1);
                Err(error)
            }
        }
    }

    pub fn finish_tv_start(&self, generation: u64) -> Result<()> {
        if self.state().generation != generation {
            return Err(Error::Cancelled);
        }
        let existing = self
            .state()
            .data
            .session
            .as_ref()
            .filter(|session| {
                session.anonymous && !session.expires_soon(self.inner.clock.seconds())
            })
            .cloned();
        let result = (|| {
            let session = match existing {
                Some(session) => session,
                None => self.inner.service.anonymous()?,
            };
            eprintln!("[account] Rust anonymous authentication completed");
            if self.state().generation != generation {
                return Err(Error::Cancelled);
            }
            let code = self.inner.service.request_code(&session)?;
            Ok((session, code))
        })();
        let mut state = self.state();
        if state.generation != generation {
            return Err(Error::Cancelled);
        }
        match result {
            Ok((session, code)) => {
                eprintln!(
                    "[account] Rust TV code ready; polling every {} seconds",
                    code.interval_seconds
                );
                let now = self.inner.clock.monotonic_millis();
                state.data = AccountData {
                    session: Some(session),
                    ..AccountData::default()
                };
                state.next_poll = now.saturating_add(code.interval_seconds.saturating_mul(1000));
                state.code_deadline = now.saturating_add(600_000);
                state.code = Some(code);
                state.login = LoginState::Waiting;
                state.error = None;
                self.persist(&mut state);
                state.revision = state.revision.wrapping_add(1);
                Ok(())
            }
            Err(error) => {
                eprintln!("[account] Rust TV code login failed: {error}");
                state.login = LoginState::Error;
                state.error = Some(error.clone());
                state.revision = state.revision.wrapping_add(1);
                Err(error)
            }
        }
    }

    /// The UI schedules work without performing network calls in its frame.
    pub fn claim_poll(&self) -> Option<u64> {
        let mut state = self.state();
        if state.login != LoginState::Waiting || state.polling {
            return None;
        }
        let now = self.inner.clock.monotonic_millis();
        if now < state.next_poll {
            return None;
        }
        if now >= state.code_deadline {
            state.login = LoginState::Error;
            state.error = Some(Error::CodeExpired);
            state.code = None;
            state.revision = state.revision.wrapping_add(1);
            return None;
        }
        state.polling = true;
        Some(state.generation)
    }
    pub fn finish_poll(&self, generation: u64) -> Result<()> {
        let (code, session) = {
            let state = self.state();
            if state.generation != generation {
                return Err(Error::Cancelled);
            }
            (
                state.code.clone().ok_or(Error::Cancelled)?,
                state.data.session.clone().ok_or(Error::Cancelled)?,
            )
        };
        let result = (|| {
            let poll = self.inner.service.poll_code(&code, &session)?;
            if poll != PollResult::Authorized {
                return Ok((poll, None));
            }
            {
                let mut state = self.state();
                if state.generation != generation {
                    return Err(Error::Cancelled);
                }
                state.login = LoginState::Exchanging;
                state.revision = state.revision.wrapping_add(1);
            }
            self.inner
                .service
                .exchange_code(&code, &session)
                .map(|session| (poll, Some(session)))
        })();
        let mut state = self.state();
        if state.generation != generation {
            return Err(Error::Cancelled);
        }
        state.polling = false;
        state.next_poll = self
            .inner
            .clock
            .monotonic_millis()
            .saturating_add(code.interval_seconds.saturating_mul(1000));
        match result {
            Ok((_, Some(session))) => {
                self.install_session(&mut state, session);
                Ok(())
            }
            Ok((PollResult::Expired, _)) => {
                state.login = LoginState::Error;
                state.error = Some(Error::CodeExpired);
                state.code = None;
                state.revision = state.revision.wrapping_add(1);
                Err(Error::CodeExpired)
            }
            Ok(_) => {
                state.login = LoginState::Waiting;
                Ok(())
            }
            Err(error) => {
                eprintln!("[account] Rust TV code poll/exchange failed: {error}");
                state.login = if is_outage(&error) {
                    LoginState::Waiting
                } else {
                    LoginState::Error
                };
                state.error = Some(error.clone());
                state.revision = state.revision.wrapping_add(1);
                Err(error)
            }
        }
    }

    pub fn cancel(&self) {
        let mut state = self.state();
        state.generation = state.generation.wrapping_add(1);
        state.selection_action = state.selection_action.wrapping_add(1);
        state.selection_result = None;
        state.code = None;
        state.polling = false;
        state.login = if state
            .data
            .session
            .as_ref()
            .is_some_and(|session| !session.anonymous)
        {
            LoginState::LoggedIn
        } else {
            LoginState::LoggedOut
        };
        state.error = None;
        state.email_error = None;
        state.revision = state.revision.wrapping_add(1);
    }
    pub fn logout(&self) {
        eprintln!("[account] Rust logout requested");
        let mut state = self.state();
        state.generation = state.generation.wrapping_add(1);
        state.selection_action = state.selection_action.wrapping_add(1);
        state.selection_result = None;
        state.data = AccountData::default();
        state.login = LoginState::LoggedOut;
        state.code = None;
        state.polling = false;
        state.error = None;
        state.email_error = None;
        state.persistence_error = self
            .inner
            .store
            .as_ref()
            .is_none_or(|store| store.clear().is_err());
        state.revision = state.revision.wrapping_add(1);
        self.inner.refreshed.notify_all();
    }

    fn install_session(&self, state: &mut State, session: Session) {
        eprintln!("[account] Rust authenticated session committed");
        if state
            .data
            .session
            .as_ref()
            .is_none_or(|previous| previous.subject != session.subject || previous.anonymous)
        {
            state.data = AccountData::default();
        }
        state.data.session = Some(session);
        state.login = LoginState::LoggedIn;
        state.code = None;
        state.error = None;
        state.email_error = None;
        self.persist(state);
        state.revision = state.revision.wrapping_add(1);
    }
    fn persist(&self, state: &mut State) {
        state.persistence_error = self
            .inner
            .store
            .as_ref()
            .is_none_or(|store| store.save(&state.data).is_err());
    }

    /// A single refresh is shared by concurrent callers, with a private token
    /// pair and generation check before publishing or discarding credentials.
    fn refresh(&self, observed: &Session, generation: u64, forced: bool) -> Result<()> {
        let session;
        {
            let mut state = self.state();
            loop {
                if state.generation != generation {
                    return Err(Error::Cancelled);
                }
                let current = state.data.session.as_ref().ok_or(Error::LoggedOut)?;
                if current.access.expose() != observed.access.expose()
                    || (!forced && !current.expires_soon(self.inner.clock.seconds()))
                {
                    return Ok(());
                }
                if !state.refreshing {
                    break;
                }
                let sequence = state.refresh_sequence;
                state = self
                    .inner
                    .refreshed
                    .wait(state)
                    .unwrap_or_else(|error| error.into_inner());
                if state.generation != generation {
                    return Err(Error::Cancelled);
                }
                if sequence != state.refresh_sequence {
                    return state.refresh_result.clone();
                }
            }
            session = state.data.session.clone().ok_or(Error::LoggedOut)?;
            state.refreshing = true;
        }
        let result = self.inner.service.refresh(&session);
        let mut state = self.state();
        let outcome = if state.generation != generation
            || state
                .data
                .session
                .as_ref()
                .is_none_or(|current| current.refresh.expose() != session.refresh.expose())
        {
            Err(Error::Cancelled)
        } else {
            match result {
                Ok(session) => {
                    eprintln!("[account] Rust session refresh completed");
                    state.data.session = Some(session);
                    self.persist(&mut state);
                    state.revision = state.revision.wrapping_add(1);
                    Ok(())
                }
                Err(error) => {
                    eprintln!("[account] Rust session refresh failed: {error}");
                    // Only an actual authentication rejection deletes a valid
                    // session. Invalid payloads and transport failures retain it.
                    if matches!(error, Error::Http(400 | 401 | 403) | Error::InvalidSession) {
                        state.generation = state.generation.wrapping_add(1);
                        state.data = AccountData::default();
                        state.login = LoginState::LoggedOut;
                        state.persistence_error = self
                            .inner
                            .store
                            .as_ref()
                            .is_none_or(|store| store.clear().is_err());
                    }
                    state.error = Some(error.clone());
                    state.revision = state.revision.wrapping_add(1);
                    Err(error)
                }
            }
        };
        state.refresh_result = outcome.clone();
        state.refreshing = false;
        state.refresh_sequence = state.refresh_sequence.wrapping_add(1);
        self.inner.refreshed.notify_all();
        outcome
    }

    fn authenticated<T>(
        &self,
        operation: impl Fn(&AccountService, &Secret) -> Result<T>,
        unauthorized: impl Fn(&T) -> bool,
    ) -> Result<T> {
        self.authenticated_generation(None, operation, unauthorized)
    }
    fn authenticated_generation<T>(
        &self,
        expected_generation: Option<u64>,
        operation: impl Fn(&AccountService, &Secret) -> Result<T>,
        unauthorized: impl Fn(&T) -> bool,
    ) -> Result<T> {
        let (session, generation) = {
            let state = self.state();
            if expected_generation.is_some_and(|expected| expected != state.generation) {
                return Err(Error::Cancelled);
            }
            (
                state
                    .data
                    .session
                    .clone()
                    .filter(|session| !session.anonymous)
                    .ok_or(Error::LoggedOut)?,
                state.generation,
            )
        };
        if session.expires_soon(self.inner.clock.seconds()) {
            self.refresh(&session, generation, false)?;
        }
        let current = self.checked_session(generation)?;
        let result = operation(&self.inner.service, &current.access);
        let needs_refresh =
            matches!(result, Err(Error::Http(401))) || result.as_ref().is_ok_and(&unauthorized);
        if needs_refresh {
            self.refresh(&current, generation, true)?;
            let current = self.checked_session(generation)?;
            let result = operation(&self.inner.service, &current.access);
            self.checked_session(generation)?;
            result
        } else {
            self.checked_session(generation)?;
            result
        }
    }
    fn checked_session(&self, generation: u64) -> Result<Session> {
        let state = self.state();
        if state.generation != generation {
            return Err(Error::Cancelled);
        }
        state
            .data
            .session
            .clone()
            .filter(|session| !session.anonymous)
            .ok_or(Error::LoggedOut)
    }
    pub fn rpc(&self, name: &str, body: &[u8]) -> Result<Response> {
        self.authenticated(
            |service, token| service.raw_rpc(name, body, token),
            |response| response.status == 401,
        )
    }
    pub fn table(&self, name: &str, query: &str) -> Result<Response> {
        self.authenticated(
            |service, token| service.table(name, query, token),
            |response| response.status == 401,
        )
    }
    pub fn content_table(
        &self,
        context: &ContentContext,
        name: &str,
        query: &str,
    ) -> Result<Response> {
        if !self.content_context_current(context) {
            return Err(Error::Cancelled);
        }
        let result = self.authenticated_generation(
            Some(context.account_generation),
            |service, token| service.table(name, query, token),
            |response| response.status == 401,
        );
        if !self.content_context_current(context) {
            return Err(Error::Cancelled);
        }
        result
    }
    pub fn content_rpc(
        &self,
        context: &ContentContext,
        name: &str,
        body: &[u8],
    ) -> Result<Response> {
        if !self.content_context_current(context) {
            return Err(Error::Cancelled);
        }
        let result = self.authenticated_generation(
            Some(context.account_generation),
            |service, token| service.raw_rpc(name, body, token),
            |response| response.status == 401,
        );
        if !self.content_context_current(context) {
            return Err(Error::Cancelled);
        }
        result
    }

    pub fn load_profiles(&self) -> Result<usize> {
        let generation = self.state().generation;
        let owner = self.authenticated(AccountService::owner, |_| false);
        let profiles = self.authenticated(AccountService::profiles, |_| false)?;
        let locks = self.authenticated(AccountService::locks, |_| false);
        let avatars = if profiles
            .iter()
            .any(|profile| profile.avatar.is_empty() && !profile.avatar_id.is_empty())
        {
            self.inner.service.avatars().unwrap_or_default()
        } else {
            Default::default()
        };
        let mut state = self.state();
        if state.generation != generation {
            return Err(Error::Cancelled);
        }
        let previous_context = (state.data.owner.clone(), state.data.selection.selected);
        if owner.as_ref().is_ok_and(|owner| {
            state
                .data
                .owner
                .as_ref()
                .is_some_and(|previous| previous != owner)
        }) {
            state.data.selection = Default::default();
            state.selection_action = state.selection_action.wrapping_add(1);
            state.selection_result = None;
        }
        let previous = &state.data.selection.profiles;
        let profiles = profiles
            .into_iter()
            .map(|mut profile| {
                if let Some(lock) = locks.as_ref().ok().and_then(|locks| locks.get(&profile.id)) {
                    profile.lock = *lock;
                } else if profile.lock == ProfileLock::Unknown {
                    profile.lock = previous
                        .iter()
                        .find(|old| old.id == profile.id)
                        .map(|old| old.lock)
                        .unwrap_or_default();
                }
                if profile.avatar.is_empty() {
                    profile.avatar = avatars.get(&profile.avatar_id).cloned().unwrap_or_default();
                }
                profile
            })
            .collect();
        if let Ok(owner) = owner {
            state.data.owner = Some(owner);
        }
        state.data.selection.profiles = profiles;
        if state.data.selection.selected.is_some_and(|id| {
            !state
                .data
                .selection
                .profiles
                .iter()
                .any(|profile| profile.id == id)
        }) {
            state.data.selection.selected = None;
            state.data.selection.confirmed = false;
            state.selection_action = state.selection_action.wrapping_add(1);
            state.selection_result = None;
        }
        if state.data.selection.selected.is_none()
            && state.data.selection.profiles.len() == 1
            && state.data.selection.profiles[0].lock == ProfileLock::Unlocked
        {
            state.data.selection.selected = Some(state.data.selection.profiles[0].id);
        }
        let count = state.data.selection.profiles.len();
        if previous_context != (state.data.owner.clone(), state.data.selection.selected) {
            state.content_generation = state.content_generation.wrapping_add(1);
        }
        eprintln!("[account] Rust profiles loaded: {count}");
        self.persist(&mut state);
        state.revision = state.revision.wrapping_add(1);
        if state.data.owner.is_none() {
            state.error = Some(Error::OwnerUnverified);
        } else if state
            .data
            .selection
            .profiles
            .iter()
            .any(|profile| profile.lock == ProfileLock::Unknown)
        {
            state.error = Some(Error::ProfileUnverified);
        } else {
            state.error = None;
        }
        Ok(count)
    }

    /// PIN verification and selection are one Rust action. No caller can
    /// claim that a locked profile was verified by setting a C flag.
    pub fn select_profile(&self, id: ProfileId, pin: Option<Secret>) -> Result<()> {
        let action = self.begin_selection();
        self.finish_selection(action, id, pin)
    }
    pub fn begin_selection(&self) -> u64 {
        let mut state = self.state();
        state.selection_action = state.selection_action.wrapping_add(1);
        state.selection_result = None;
        state.revision = state.revision.wrapping_add(1);
        state.selection_action
    }
    pub fn cancel_selection(&self) {
        let mut state = self.state();
        state.selection_action = state.selection_action.wrapping_add(1);
        state.selection_result = None;
        state.revision = state.revision.wrapping_add(1);
    }
    pub fn finish_selection(&self, action: u64, id: ProfileId, pin: Option<Secret>) -> Result<()> {
        let result = self.select_checked(action, id, pin);
        let mut state = self.state();
        if state.selection_action == action {
            state.selection_result = Some(result.clone());
            state.revision = state.revision.wrapping_add(1);
        }
        result
    }
    fn select_checked(&self, action: u64, id: ProfileId, pin: Option<Secret>) -> Result<()> {
        let (generation, needs_pin) = {
            let state = self.state();
            if state.selection_action != action {
                return Err(Error::Cancelled);
            }
            let profile = state
                .data
                .selection
                .profiles
                .iter()
                .find(|profile| profile.id == id)
                .ok_or(Error::InvalidData)?;
            if profile.lock == ProfileLock::Unknown {
                return Err(Error::ProfileUnverified);
            }
            (state.generation, profile.lock == ProfileLock::Locked)
        };
        if needs_pin {
            let pin = pin.ok_or(Error::WrongPin)?;
            if !self.authenticated(
                |service, token| service.verify_pin(id, &pin, token),
                |_| false,
            )? {
                return Err(Error::WrongPin);
            }
        }
        let mut state = self.state();
        if state.generation != generation || state.selection_action != action {
            return Err(Error::Cancelled);
        }
        let previous = state.data.selection.selected;
        state.data.selection.select(id, needs_pin)?;
        if previous != Some(id) {
            state.content_generation = state.content_generation.wrapping_add(1);
        }
        self.persist(&mut state);
        state.revision = state.revision.wrapping_add(1);
        Ok(())
    }
    pub fn keep_profile(&self) -> Result<()> {
        let mut state = self.state();
        if !state.data.selection.can_dismiss() {
            return Err(Error::ProfileUnverified);
        }
        state.data.selection.confirmed = true;
        state.revision = state.revision.wrapping_add(1);
        Ok(())
    }
}

fn is_outage(error: &Error) -> bool {
    matches!(
        error,
        Error::Network | Error::Timeout | Error::Tls | Error::Http(429 | 500..=599)
    )
}
