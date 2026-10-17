//! Per-profile recent searches. Frames only read memory; one worker owns disk I/O.
use crate::{
    account::AccountBackend,
    content_context::ContentScope,
    error::{Error, Result},
    search_history::{SearchHistory, SearchHistoryStore},
    search_text::history_key,
};
use std::{
    collections::{HashMap, VecDeque},
    sync::{
        atomic::{AtomicU64, Ordering},
        Arc, Condvar, Mutex,
    },
    thread::{self, JoinHandle},
    time::{Duration, Instant},
};
pub trait HistoryAccount: Send + Sync {
    fn history_scope(&self) -> Option<ContentScope>;
}
impl HistoryAccount for AccountBackend {
    fn history_scope(&self) -> Option<ContentScope> {
        self.content_context(false).as_ref().map(ContentScope::from)
    }
}
pub trait HistoryStorage: Send + Sync {
    fn load(&self, scope: &ContentScope, current: &dyn Fn() -> bool) -> Result<SearchHistory>;
    fn save(
        &self,
        scope: &ContentScope,
        history: &SearchHistory,
        current: &dyn Fn() -> bool,
    ) -> Result<()>;
    fn clear(&self) -> Result<()>;
}
impl HistoryStorage for SearchHistoryStore {
    fn load(&self, scope: &ContentScope, current: &dyn Fn() -> bool) -> Result<SearchHistory> {
        SearchHistoryStore::load(self, scope, current)
    }
    fn save(
        &self,
        scope: &ContentScope,
        history: &SearchHistory,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        SearchHistoryStore::save(self, scope, history, current)
    }
    fn clear(&self) -> Result<()> {
        SearchHistoryStore::clear(self)
    }
}
#[derive(Clone)]
enum Edit {
    Record(String),
    Remove(String),
    Clear,
}
impl Edit {
    fn apply(&self, history: &mut SearchHistory) {
        match self {
            Self::Record(query) => {
                history.record(query);
            }
            Self::Remove(key) => {
                let index = history
                    .queries()
                    .iter()
                    .position(|query| history_key(query) == *key);
                if let Some(index) = index {
                    history.remove(index);
                }
            }
            Self::Clear => history.clear(),
        }
    }
}
#[derive(Default)]
struct Entry {
    history: SearchHistory,
    loaded: bool,
    edits: Vec<Edit>,
    dirty: bool,
    queued: bool,
    retry: Option<Instant>,
    error: Option<Error>,
}
enum Work {
    Update(ContentScope, u64),
    Clear,
}
struct State {
    active: Option<ContentScope>,
    entries: HashMap<ContentScope, Entry>,
    pending: VecDeque<Work>,
    revision: u64,
    stopped: bool,
}
struct Shared {
    state: Mutex<State>,
    ready: Condvar,
    epoch: AtomicU64,
}
#[derive(Clone, Debug)]
pub struct HistorySnapshot {
    pub revision: u64,
    pub scope: Option<ContentScope>,
    pub history: SearchHistory,
    pub loading: bool,
    pub error: Option<Error>,
}
pub struct SearchHistoryRuntime {
    account: Arc<dyn HistoryAccount>,
    shared: Arc<Shared>,
    thread: Option<JoinHandle<()>>,
}
impl SearchHistoryRuntime {
    pub fn new(account: Arc<dyn HistoryAccount>, storage: Arc<dyn HistoryStorage>) -> Result<Self> {
        let shared = Arc::new(Shared {
            state: Mutex::new(State {
                active: None,
                entries: HashMap::new(),
                pending: VecDeque::new(),
                revision: 1,
                stopped: false,
            }),
            ready: Condvar::new(),
            epoch: AtomicU64::new(1),
        });
        let worker = shared.clone();
        let thread = thread::Builder::new()
            .name("nuvio-search-history".into())
            .spawn(move || loop {
                let work = {
                    let mut state = worker.state.lock().unwrap();
                    while state.pending.is_empty() && !state.stopped {
                        state = worker.ready.wait(state).unwrap();
                    }
                    match state.pending.pop_front() {
                        Some(work) => work,
                        None => break,
                    }
                };
                let Work::Update(scope, epoch) = work else {
                    if let Err(error) = storage.clear() {
                        println!("[search] Could not clear recent searches: {error}");
                    }
                    continue;
                };
                let current = || worker.epoch.load(Ordering::Acquire) == epoch;
                let needs_load = {
                    let state = worker.state.lock().unwrap();
                    if !current() {
                        continue;
                    }
                    !state.entries.get(&scope).is_some_and(|entry| entry.loaded)
                };
                let loaded = needs_load.then(|| storage.load(&scope, &current));
                let save = {
                    let mut state = worker.state.lock().unwrap();
                    if !current() {
                        continue;
                    }
                    let Some(entry) = state.entries.get_mut(&scope) else {
                        continue;
                    };
                    entry.queued = false;
                    if let Some(loaded) = loaded {
                        match loaded {
                            Ok(mut history) => {
                                for edit in entry.edits.drain(..) {
                                    edit.apply(&mut history);
                                }
                                entry.history = history;
                                entry.loaded = true;
                                entry.error = None;
                                entry.retry = None;
                            }
                            Err(error) => {
                                // Protect an unreadable Rust history file. Edits remain in memory
                                // until a later successful load can merge and persist them.
                                entry.error = Some(error);
                                entry.retry = Some(Instant::now() + Duration::from_secs(30));
                            }
                        }
                    }
                    let save = (entry.loaded && entry.dirty).then(|| entry.history.clone());
                    state.revision += 1;
                    save
                };
                if let Some(history) = save {
                    let result = storage.save(&scope, &history, &current);
                    let mut state = worker.state.lock().unwrap();
                    if !current() {
                        continue;
                    }
                    if let Some(entry) = state.entries.get_mut(&scope) {
                        match result {
                            Ok(()) => {
                                // An edit arriving during the write has its own queued update.
                                entry.dirty = entry.history != history;
                                entry.error = None;
                                entry.retry = None;
                            }
                            Err(error) => {
                                entry.error = Some(error);
                                entry.retry = Some(Instant::now() + Duration::from_secs(30));
                            }
                        }
                        if let Some(error) = &entry.error {
                            println!("[search] Recent searches will retry: {error}");
                        }
                    }
                    state.revision += 1;
                }
            })
            .map_err(|_| Error::Storage)?;
        Ok(Self {
            account,
            shared,
            thread: Some(thread),
        })
    }
    fn queue(shared: &Shared, state: &mut State, scope: &ContentScope) {
        let entry = state.entries.entry(scope.clone()).or_default();
        if !entry.queued
            && entry.retry.is_none_or(|retry| Instant::now() >= retry)
            && (!entry.loaded || entry.dirty)
        {
            entry.queued = true;
            state.pending.push_back(Work::Update(
                scope.clone(),
                shared.epoch.load(Ordering::Acquire),
            ));
            shared.ready.notify_one();
        }
    }
    pub fn step(&self) {
        let scope = self.account.history_scope();
        let mut state = self.shared.state.lock().unwrap();
        if state.active != scope {
            state.active = scope.clone();
            state.revision += 1;
        }
        if let Some(scope) = scope {
            Self::queue(&self.shared, &mut state, &scope);
        }
        // Outgoing profiles keep their own unsaved changes across a switch.
        let retry: Vec<_> = state
            .entries
            .iter()
            .filter(|(_, entry)| entry.dirty)
            .map(|(scope, _)| scope.clone())
            .collect();
        for scope in retry {
            Self::queue(&self.shared, &mut state, &scope);
        }
    }
    pub fn revision(&self) -> u64 {
        self.shared.state.lock().unwrap().revision
    }
    pub fn snapshot(&self) -> HistorySnapshot {
        let state = self.shared.state.lock().unwrap();
        let entry = state
            .active
            .as_ref()
            .and_then(|scope| state.entries.get(scope));
        HistorySnapshot {
            revision: state.revision,
            scope: state.active.clone(),
            history: entry.map(|entry| entry.history.clone()).unwrap_or_default(),
            loading: entry.is_some_and(|entry| !entry.loaded),
            error: entry.and_then(|entry| entry.error.clone()),
        }
    }
    fn edit(&self, edit: Edit) -> bool {
        self.step();
        let mut state = self.shared.state.lock().unwrap();
        let Some(scope) = state.active.clone() else {
            return false;
        };
        let entry = state.entries.entry(scope.clone()).or_default();
        let previous = entry.history.clone();
        edit.apply(&mut entry.history);
        if !entry.loaded {
            entry.edits.push(edit);
        }
        // Clear/remove before restoration must still apply to the disk snapshot.
        entry.dirty |= !entry.loaded || entry.history != previous;
        state.revision += 1;
        Self::queue(&self.shared, &mut state, &scope);
        true
    }
    pub fn record(&self, query: &str) -> bool {
        let mut valid = SearchHistory::default();
        if !valid.record(query) {
            return false;
        }
        self.edit(Edit::Record(valid.queries()[0].clone()))
    }
    pub fn remove(&self, index: usize) {
        self.step();
        let query = self.snapshot().history.queries().get(index).cloned();
        if let Some(query) = query {
            self.edit(Edit::Remove(history_key(&query)));
        }
    }
    pub fn clear_current(&self) {
        self.edit(Edit::Clear);
    }
    pub fn logout(&self) {
        let mut state = self.shared.state.lock().unwrap();
        self.shared.epoch.fetch_add(1, Ordering::AcqRel);
        state.active = None;
        state.entries.clear();
        state.pending.clear();
        state.pending.push_back(Work::Clear);
        state.revision += 1;
        self.shared.ready.notify_one();
    }
}
impl Drop for SearchHistoryRuntime {
    fn drop(&mut self) {
        {
            let mut state = self.shared.state.lock().unwrap();
            state.stopped = true;
            self.shared.ready.notify_one();
        }
        // Shutdown drains confirmed edits; frames never wait for this worker.
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}
