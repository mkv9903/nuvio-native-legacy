//! Account Home settings with selected-profile ownership and guarded results.
use crate::{
    account::AccountBackend,
    catalog_order::{HomeOrder, HomeOrderStorage},
    content_context::{ContentContext, ContentScope},
    error::{Error, Result},
};
use serde::Serialize;
use std::{
    collections::HashMap,
    sync::{Arc, Mutex},
};

pub trait HomeOrderAccount: Send + Sync {
    fn current_context(&self) -> Option<ContentContext>;
    fn is_current(&self, context: &ContentContext) -> bool;
    fn read_order(&self, context: &ContentContext) -> Result<Option<HomeOrder>>;
}
impl HomeOrderAccount for AccountBackend {
    fn current_context(&self) -> Option<ContentContext> {
        self.content_context(false)
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.content_context_current(context)
    }
    fn read_order(&self, context: &ContentContext) -> Result<Option<HomeOrder>> {
        #[derive(Serialize)]
        struct Pull {
            p_profile_id: u32,
            p_platform: &'static str,
        }
        let body = serde_json::to_vec(&Pull {
            p_profile_id: context.profile.get(),
            p_platform: "home_catalog_shared",
        })
        .map_err(|_| Error::InvalidData)?;
        let response = self.content_rpc(context, "sync_pull_home_catalog_settings", &body)?;
        response.success()?;
        HomeOrder::decode(&response.body)
    }
}
#[derive(Clone, Debug)]
pub struct HomeOrderSnapshot {
    pub revision: u64,
    pub context: Option<ContentContext>,
    pub order: Arc<HomeOrder>,
    pub error: Option<Error>,
    pub storage_error: Option<Error>,
}
#[derive(Default)]
struct ProfileState {
    restored: bool,
    order: Arc<HomeOrder>,
    error: Option<Error>,
    storage_error: Option<Error>,
}
struct State {
    active: Option<ContentContext>,
    profiles: HashMap<ContentScope, ProfileState>,
    published: HomeOrderSnapshot,
}
impl State {
    fn publish(&mut self) {
        let profile = self
            .active
            .as_ref()
            .and_then(|context| self.profiles.get(&context.into()));
        self.published = HomeOrderSnapshot {
            revision: self.published.revision.wrapping_add(1),
            context: self.active.clone(),
            order: profile
                .map(|profile| profile.order.clone())
                .unwrap_or_default(),
            error: profile.and_then(|profile| profile.error.clone()),
            storage_error: profile.and_then(|profile| profile.storage_error.clone()),
        };
    }
}
struct Inner {
    account: Arc<dyn HomeOrderAccount>,
    storage: Arc<dyn HomeOrderStorage>,
    state: Mutex<State>,
    operation: Mutex<()>,
}
#[derive(Clone)]
pub struct HomeOrderBackend(Arc<Inner>);
impl HomeOrderBackend {
    pub fn new(account: Arc<dyn HomeOrderAccount>, storage: Arc<dyn HomeOrderStorage>) -> Self {
        Self(Arc::new(Inner {
            account,
            storage,
            operation: Mutex::new(()),
            state: Mutex::new(State {
                active: None,
                profiles: HashMap::new(),
                published: HomeOrderSnapshot {
                    revision: 0,
                    context: None,
                    order: Arc::default(),
                    error: None,
                    storage_error: None,
                },
            }),
        }))
    }
    /// Frame work selects only an in-memory snapshot; no disk or network I/O.
    pub fn activate(&self) -> Option<ContentContext> {
        let context = self.0.account.current_context();
        let mut state = self.0.state.lock().unwrap();
        if state.active != context {
            if let Some(context) = &context {
                state.profiles.entry(context.into()).or_default();
            }
            state.active = context.clone();
            state.publish();
        }
        context
    }
    pub fn snapshot(&self) -> HomeOrderSnapshot {
        self.0.state.lock().unwrap().published.clone()
    }
    pub fn snapshot_current(&self, snapshot: &HomeOrderSnapshot) -> bool {
        let state = self.0.state.lock().unwrap();
        snapshot.context == state.active
            && snapshot
                .context
                .as_ref()
                .map(|context| self.0.account.is_current(context))
                .unwrap_or_else(|| self.0.account.current_context().is_none())
    }
    fn current(&self, context: &ContentContext) -> bool {
        self.0.account.is_current(context)
            && self.0.state.lock().unwrap().active.as_ref() == Some(context)
    }
    fn require_current(&self, context: &ContentContext) -> Result<()> {
        if self.current(context) {
            Ok(())
        } else {
            Err(Error::Cancelled)
        }
    }
    fn update(
        &self,
        context: &ContentContext,
        change: impl FnOnce(&mut ProfileState),
    ) -> Result<()> {
        let mut state = self.0.state.lock().unwrap();
        if state.active.as_ref() != Some(context) || !self.0.account.is_current(context) {
            return Err(Error::Cancelled);
        }
        change(
            state
                .profiles
                .get_mut(&context.into())
                .expect("active Home scope"),
        );
        state.publish();
        Ok(())
    }
    pub fn synchronize(&self, context: &ContentContext) -> Result<()> {
        let _operation = self.0.operation.lock().unwrap();
        self.require_current(context)?;
        let scope = ContentScope::from(context);
        let restored = self.0.state.lock().unwrap().profiles[&scope].restored;
        if !restored {
            let cached = self.0.storage.load(&scope);
            self.update(context, |profile| match cached {
                Ok(order) => {
                    profile.restored = true;
                    if let Some(order) = order {
                        profile.order = Arc::new(order);
                    }
                    profile.storage_error = None;
                }
                Err(error) => profile.storage_error = Some(error),
            })?;
        }
        self.require_current(context)?;
        let response = self.0.account.read_order(context);
        self.require_current(context)?;
        let order = match response {
            Ok(Some(mut order)) => {
                let previous = self.snapshot().order;
                order.hide_unreleased = order.hide_unreleased.or(previous.hide_unreleased);
                order.hide_underline = order.hide_underline.or(previous.hide_underline);
                order
            }
            Ok(None) => {
                // Absent settings do not discard a valid cached configuration.
                self.update(context, |profile| profile.error = None)?;
                return Ok(());
            }
            Err(error) => {
                self.update(context, |profile| profile.error = Some(error.clone()))?;
                return Err(error);
            }
        };
        let saved = self
            .0
            .storage
            .save(&scope, &order, &|| self.current(context));
        self.require_current(context)?;
        let storage_error = saved.err();
        self.update(context, |profile| {
            profile.order = Arc::new(order);
            profile.restored = true;
            profile.error = None;
            profile.storage_error = storage_error.clone();
        })?;
        // A storage failure cannot hide a usable server reply, but needs retry.
        storage_error.map_or(Ok(()), Err)
    }
    pub fn forget_runtime(&self) {
        let mut state = self.0.state.lock().unwrap();
        state.active = None;
        state.profiles.clear();
        state.publish();
    }
    pub fn clear(&self) -> Result<()> {
        let _operation = self.0.operation.lock().unwrap();
        self.0.storage.clear()
    }
}
