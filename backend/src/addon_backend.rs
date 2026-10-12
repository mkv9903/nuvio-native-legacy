//! Profile-scoped addon state. UI actions touch memory; workers own I/O.
use crate::{
    addon_edits::{AddonChange, AddonEdit},
    addon_service::AddonAccount,
    addon_snapshot::{AddonRecord, AddonSnapshot},
    addon_storage::AddonStorage,
    addons::{AddonUrl, InstalledAddon, Manifest},
    content_context::{AddonScope, ContentContext},
    error::{Error, Result},
};
use std::{
    collections::HashMap,
    sync::{Arc, Mutex, MutexGuard},
};
use uuid::Uuid;

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AddonStatus {
    Unavailable,
    Loading,
    Ready,
    RetryPending,
    Unsaved,
    RecoveryPaused,
}
#[derive(Clone, Debug)]
pub struct AddonBackendSnapshot {
    pub addons: AddonSnapshot,
    pub status: AddonStatus,
    pub error: Option<Error>,
}
#[derive(Clone, Debug)]
enum Restore {
    Waiting,
    Ready,
    Paused,
}
struct ProfileState {
    restore: Restore,
    cached: Option<Vec<InstalledAddon>>,
    pending: Option<AddonEdit>,
    persisted: Option<Uuid>,
    manifests: HashMap<AddonUrl, Arc<Manifest>>,
    status: AddonStatus,
    error: Option<Error>,
}
impl ProfileState {
    fn new() -> Self {
        Self {
            restore: Restore::Waiting,
            cached: None,
            pending: None,
            persisted: None,
            manifests: HashMap::new(),
            status: AddonStatus::Loading,
            error: None,
        }
    }
    fn installed(&self) -> Vec<InstalledAddon> {
        let cached = self.cached.as_deref().unwrap_or_default();
        self.pending
            .as_ref()
            .map(|edit| edit.merge(cached))
            .unwrap_or_else(|| cached.to_vec())
    }
}
struct State {
    active: Option<ContentContext>,
    profiles: HashMap<AddonScope, ProfileState>,
    published: Arc<AddonBackendSnapshot>,
}
impl State {
    fn publish(&mut self) {
        let profile = self
            .active
            .as_ref()
            .and_then(|context| self.profiles.get(&context.addons));
        let installed = profile.map(ProfileState::installed).unwrap_or_default();
        let previous = &self.published.addons;
        let list_changed = previous.context != self.active
            || !previous
                .records
                .iter()
                .map(|record| &record.installed)
                .eq(installed.iter());
        let records = installed
            .into_iter()
            .map(|installed| AddonRecord {
                manifest: profile
                    .and_then(|profile| profile.manifests.get(&installed.url))
                    .cloned(),
                installed,
                silent_queries: 0,
            })
            .collect::<Vec<_>>()
            .into();
        self.published = Arc::new(AddonBackendSnapshot {
            addons: AddonSnapshot {
                context: self.active.clone(),
                revision: previous.revision.wrapping_add(1),
                list_revision: previous.list_revision.wrapping_add(u64::from(list_changed)),
                records,
                authoritative: profile.is_some_and(|profile| profile.cached.is_some()),
                pending: profile.is_some_and(|profile| profile.pending.is_some()),
            },
            status: profile
                .map(|profile| profile.status.clone())
                .unwrap_or(AddonStatus::Unavailable),
            error: profile.and_then(|profile| profile.error.clone()),
        });
    }
}
struct Inner {
    account: Arc<dyn AddonAccount>,
    storage: Arc<dyn AddonStorage>,
    state: Mutex<State>,
    operation: Mutex<()>,
}
#[derive(Clone)]
pub struct AddonBackend(Arc<Inner>);
impl AddonBackend {
    pub fn new(account: Arc<dyn AddonAccount>, storage: Arc<dyn AddonStorage>) -> Self {
        let published = Arc::new(AddonBackendSnapshot {
            addons: AddonSnapshot {
                context: None,
                revision: 0,
                list_revision: 0,
                records: Arc::from([]),
                authoritative: false,
                pending: false,
            },
            status: AddonStatus::Unavailable,
            error: None,
        });
        Self(Arc::new(Inner {
            account,
            storage,
            state: Mutex::new(State {
                active: None,
                profiles: HashMap::new(),
                published,
            }),
            operation: Mutex::new(()),
        }))
    }
    fn state(&self) -> MutexGuard<'_, State> {
        self.0
            .state
            .lock()
            .unwrap_or_else(|error| error.into_inner())
    }
    /// Called at UI frame/action boundaries. Switching never waits for disk or network.
    pub fn activate(&self, force_primary: bool) -> Option<ContentContext> {
        let context = self.0.account.current_context(force_primary);
        let mut state = self.state();
        if state.active != context {
            if let Some(context) = &context {
                state
                    .profiles
                    .entry(context.addons.clone())
                    .or_insert_with(ProfileState::new);
            }
            state.active = context.clone();
            state.publish();
        }
        context
    }
    pub fn snapshot(&self) -> Arc<AddonBackendSnapshot> {
        self.state().published.clone()
    }
    fn current(&self, context: &ContentContext) -> Result<()> {
        if self.0.account.is_current(context) && self.state().active.as_ref() == Some(context) {
            Ok(())
        } else {
            Err(Error::Cancelled)
        }
    }
    fn check_state(&self, state: &State, context: &ContentContext) -> Result<()> {
        if state.active.as_ref() == Some(context) && self.0.account.is_current(context) {
            Ok(())
        } else {
            Err(Error::Cancelled)
        }
    }
    pub fn forget_runtime(&self) {
        let mut state = self.state();
        state.active = None;
        state.profiles.clear();
        state.publish();
    }
    fn fail(&self, context: &ContentContext, error: Error, paused: bool) -> Error {
        let mut state = self.state();
        if state.active.as_ref() == Some(context) && self.0.account.is_current(context) {
            if let Some(profile) = state.profiles.get_mut(&context.addons) {
                profile.error = Some(error.clone());
                profile.status = if paused {
                    AddonStatus::RecoveryPaused
                } else if profile.pending.as_ref().map(|edit| edit.id) != profile.persisted {
                    AddonStatus::Unsaved
                } else {
                    AddonStatus::RetryPending
                };
                if paused {
                    profile.restore = Restore::Paused;
                }
            }
            state.publish();
        }
        error
    }
    fn restore(&self, context: &ContentContext) -> Result<()> {
        self.current(context)?;
        {
            let state = self.state();
            self.check_state(&state, context)?;
            let profile = &state.profiles[&context.addons];
            if matches!(profile.restore, Restore::Ready) {
                return Ok(());
            }
            // A cross-process conflict must not discard unsaved in-memory intentions.
            if profile.error == Some(Error::AddonEditsChanged) {
                return Err(Error::AddonEditsChanged);
            }
        }
        let stored = self.0.storage.load_state(&context.addons);
        self.current(context)?;
        match stored {
            Ok(stored) => {
                let mut state = self.state();
                self.check_state(&state, context)?;
                let profile = state
                    .profiles
                    .get_mut(&context.addons)
                    .expect("active addon profile");
                if profile.pending.is_some() {
                    // A temporarily unreadable journal must not erase RAM edits on retry.
                    if stored.pending.as_ref().map(|edit| edit.id) != profile.persisted {
                        drop(state);
                        return Err(self.fail(context, Error::AddonEditsChanged, true));
                    }
                } else {
                    profile.cached = stored.cached;
                    profile.persisted = stored.pending.as_ref().map(|edit| edit.id);
                    profile.pending = stored.pending;
                }
                profile.error = stored.cache_error;
                profile.restore = Restore::Ready;
                profile.status = AddonStatus::Loading;
                state.publish();
                Ok(())
            }
            Err(error) => {
                // A valid cache may still be displayed; the pending file is left untouched.
                let cached = self.0.storage.load_cache(&context.addons).ok().flatten();
                self.current(context)?;
                let mut state = self.state();
                self.check_state(&state, context)?;
                state
                    .profiles
                    .get_mut(&context.addons)
                    .expect("active addon profile")
                    .cached = cached;
                drop(state);
                Err(self.fail(context, error, true))
            }
        }
    }
    fn action(&self, snapshot: &AddonSnapshot, change: AddonChange) -> Result<()> {
        let context = snapshot.context.as_ref().ok_or(Error::OwnerUnverified)?;
        let mut state = self.state();
        if state.active.as_ref() != Some(context)
            || !self.0.account.is_current(context)
            || state.published.addons.list_revision != snapshot.list_revision
            || !state
                .published
                .addons
                .records
                .iter()
                .map(|record| &record.installed)
                .eq(snapshot.records.iter().map(|record| &record.installed))
        {
            return Err(Error::Cancelled);
        }
        let profile = state
            .profiles
            .get_mut(&context.addons)
            .expect("active addon profile");
        if !matches!(profile.restore, Restore::Ready) {
            return Err(profile.error.clone().unwrap_or(Error::ProfileUnverified));
        }
        if let Some(edit) = &mut profile.pending {
            edit.append(change);
        } else {
            profile.pending = Some(AddonEdit::new(change));
        }
        profile.status = AddonStatus::Unsaved;
        profile.error = None;
        state.publish();
        Ok(())
    }
    pub fn set_enabled(&self, snapshot: &AddonSnapshot, index: usize, enabled: bool) -> Result<()> {
        let record = snapshot.records.get(index).ok_or(Error::InvalidData)?;
        self.action(
            snapshot,
            AddonChange::SetEnabled {
                url: record.installed.url.clone(),
                enabled,
            },
        )
    }
    pub fn install(&self, snapshot: &AddonSnapshot, addon: InstalledAddon) -> Result<bool> {
        if snapshot
            .records
            .iter()
            .any(|record| record.installed.url == addon.url)
        {
            // Validate even a no-op action against the selected profile.
            let context = snapshot.context.as_ref().ok_or(Error::OwnerUnverified)?;
            self.current(context)?;
            if self.snapshot().addons.list_revision != snapshot.list_revision {
                return Err(Error::Cancelled);
            }
            return Ok(false);
        }
        self.action(snapshot, AddonChange::Install { addon })?;
        Ok(true)
    }
    fn persist(&self, context: &ContentContext) -> Result<()> {
        self.current(context)?;
        self.persist_scope(&context.addons)
    }
    fn persist_scope(&self, scope: &AddonScope) -> Result<()> {
        let (pending, previous) = {
            let state = self.state();
            let Some(profile) = state.profiles.get(scope) else {
                return Ok(());
            };
            if !matches!(profile.restore, Restore::Ready) {
                return Ok(());
            }
            (profile.pending.clone(), profile.persisted)
        };
        let Some(pending) = pending.filter(|edit| Some(edit.id) != previous) else {
            return Ok(());
        };
        match self.0.storage.replace_edit(scope, previous, &pending) {
            Ok(true) => {
                // Track a completed write even if selection changed while disk I/O ran.
                let mut state = self.state();
                if let Some(profile) = state.profiles.get_mut(scope) {
                    profile.persisted = Some(pending.id);
                }
                Ok(())
            }
            result => {
                let error = result.err().unwrap_or(Error::AddonEditsChanged);
                let paused = matches!(
                    error,
                    Error::AddonJournalUnreadable | Error::AddonEditsChanged
                );
                let mut state = self.state();
                if let Some(profile) = state.profiles.get_mut(scope) {
                    profile.error = Some(error.clone());
                    profile.status = if paused {
                        AddonStatus::RecoveryPaused
                    } else {
                        AddonStatus::Unsaved
                    };
                    if paused {
                        profile.restore = Restore::Paused;
                    }
                }
                state.publish();
                Err(error)
            }
        }
    }
    fn flush_locked(&self) -> Result<()> {
        let scopes: Vec<_> = self.state().profiles.keys().cloned().collect();
        let mut failure = None;
        for scope in scopes {
            if let Err(error) = self.persist_scope(&scope) {
                failure.get_or_insert(error);
            }
        }
        failure.map_or(Ok(()), Err)
    }
    /// Persist accepted edits even if a profile switch cancels their network job.
    pub fn flush_edits(&self) -> Result<()> {
        let _operation = self
            .0
            .operation
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        self.flush_locked()
    }
    /// Only the addon worker calls this method. Reads precede every upload; a
    /// read failure never uploads an old cached replacement. The existing server
    /// RPC is not atomic with this read, so simultaneous external writes can race.
    pub fn synchronize(&self, context: &ContentContext) -> Result<()> {
        let _operation = self
            .0
            .operation
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        // An inactive profile's disk failure must not prevent loading another profile.
        let _ = self.flush_locked();
        self.restore(context)?;
        self.persist(context)?;
        self.current(context)?;
        let latest = self
            .0
            .account
            .read_addons(context)
            .map_err(|error| self.fail(context, error, false))?;
        self.current(context)?;
        {
            let mut state = self.state();
            self.check_state(&state, context)?;
            let profile = state
                .profiles
                .get_mut(&context.addons)
                .expect("active addon profile");
            profile.cached = Some(latest.clone());
            profile.error = None;
            state.publish();
        }
        if let Err(error) = self.0.storage.save_cache(&context.addons, &latest) {
            self.fail(context, error, false);
        }
        // Edits made while the GET was in flight must also be durable before upload.
        self.persist(context)?;
        self.current(context)?;
        let pending = {
            let state = self.state();
            self.check_state(&state, context)?;
            let profile = &state.profiles[&context.addons];
            profile
                .pending
                .clone()
                .filter(|edit| Some(edit.id) == profile.persisted)
        };
        if let Some(pending) = pending {
            let merged = pending.merge(&latest);
            self.0
                .account
                .write_addons(context, &merged)
                .map_err(|error| self.fail(context, error, false))?;
            self.current(context)?;
            let acknowledged = self
                .0
                .storage
                .acknowledge(&context.addons, pending.id, &merged)
                .map_err(|error| {
                    let paused = error == Error::AddonJournalUnreadable;
                    self.fail(context, error, paused)
                })?;
            if !acknowledged {
                return Err(self.fail(context, Error::AddonEditsChanged, true));
            }
            let mut state = self.state();
            self.check_state(&state, context)?;
            let profile = state
                .profiles
                .get_mut(&context.addons)
                .expect("active addon profile");
            profile.cached = Some(merged);
            profile.error = None;
            if profile.persisted == Some(pending.id) {
                profile.persisted = None;
            }
            if profile
                .pending
                .as_ref()
                .is_some_and(|edit| edit.id == pending.id)
            {
                profile.pending = None;
            }
            state.publish();
            drop(state);
            // An action submitted during the upload remains pending with a new ID.
            self.persist(context)?;
        }
        self.current(context)?;
        let mut state = self.state();
        self.check_state(&state, context)?;
        let profile = state
            .profiles
            .get_mut(&context.addons)
            .expect("active addon profile");
        profile.status = if profile.pending.as_ref().map(|edit| edit.id) != profile.persisted {
            AddonStatus::Unsaved
        } else if profile.pending.is_some() {
            AddonStatus::RetryPending
        } else if profile.error.is_some() {
            AddonStatus::RetryPending
        } else {
            AddonStatus::Ready
        };
        state.publish();
        Ok(())
    }
    pub fn attach_manifest(
        &self,
        context: &ContentContext,
        url: &AddonUrl,
        manifest: Manifest,
    ) -> Result<()> {
        let mut state = self.state();
        if state.active.as_ref() != Some(context) || !self.0.account.is_current(context) {
            return Err(Error::Cancelled);
        }
        let profile = state
            .profiles
            .get_mut(&context.addons)
            .expect("active addon profile");
        if !profile.installed().iter().any(|addon| &addon.url == url) {
            return Err(Error::Cancelled);
        }
        profile.manifests.insert(url.clone(), Arc::new(manifest));
        state.publish();
        Ok(())
    }
    /// Run on the worker after logout; no UI thread performs filesystem cleanup.
    pub fn clear(&self) -> Result<()> {
        let _operation = self
            .0
            .operation
            .lock()
            .unwrap_or_else(|error| error.into_inner());
        {
            let mut state = self.state();
            state.active = None;
            state.profiles.clear();
            state.publish();
        }
        self.0.storage.clear()
    }
}
