//! Profile-scoped scene restoration and publication with stale-work guards.
use crate::{
    addon_backend::{AddonBackend, AddonBackendSnapshot},
    addon_snapshot::AddonSnapshot,
    catalog_scene::CatalogScene,
    content_context::ContentScope,
    error::{Error, Result},
    scene_cache::SceneCache,
    scene_snapshot::{RowSnapshot, SceneSnapshot},
};
use std::{
    collections::HashMap,
    sync::{Arc, Mutex, MutexGuard},
};

pub trait SceneSource: Send + Sync {
    fn snapshot(&self) -> Arc<AddonBackendSnapshot>;
    fn current(&self, source: &AddonSnapshot) -> bool;
}
impl SceneSource for AddonBackend {
    fn snapshot(&self) -> Arc<AddonBackendSnapshot> {
        AddonBackend::snapshot(self)
    }
    fn current(&self, source: &AddonSnapshot) -> bool {
        self.snapshot_current(source)
    }
}
pub trait SceneStorage: Send + Sync {
    fn home_plan_keys(
        &self,
        _scope: &ContentScope,
        _current: &dyn Fn() -> bool,
    ) -> Result<Vec<String>> {
        Ok(Vec::new())
    }
    fn save_home_plan(
        &self,
        _scope: &ContentScope,
        _keys: &[String],
        _current: &dyn Fn() -> bool,
    ) -> Result<()> {
        Ok(())
    }
    fn load(
        &self,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Option<Arc<SceneSnapshot>>>;
    fn save(
        &self,
        scope: &ContentScope,
        scene: &SceneSnapshot,
        current: &dyn Fn() -> bool,
    ) -> Result<()>;
    fn clear(&self) -> Result<()>;
}
impl SceneStorage for SceneCache {
    fn home_plan_keys(
        &self,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Vec<String>> {
        SceneCache::home_plan_keys(self, scope, current)
    }
    fn save_home_plan(
        &self,
        scope: &ContentScope,
        keys: &[String],
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        SceneCache::save_home_plan(self, scope, keys, current)
    }
    fn load(
        &self,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Option<Arc<SceneSnapshot>>> {
        SceneSnapshot::restore(self, scope, current).map(|scene| scene.map(Arc::new))
    }
    fn save(
        &self,
        scope: &ContentScope,
        scene: &SceneSnapshot,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        SceneCache::save(self, scope, &scene.to_scene(), current)
    }
    fn clear(&self) -> Result<()> {
        SceneCache::clear(self)
    }
}
#[derive(Clone, Copy, PartialEq, Eq)]
enum WorkKind {
    Restore,
    Publish,
}
#[derive(Clone)]
pub struct SceneWork {
    pub(crate) source: Arc<AddonBackendSnapshot>,
    epoch: u64,
    sequence: u64,
    version: u64,
    kind: WorkKind,
}
impl SceneWork {
    pub fn scope(&self) -> ContentScope {
        ContentScope::from(
            self.source
                .addons
                .context
                .as_ref()
                .expect("verified scene context"),
        )
    }
}
pub struct SceneView {
    pub source: Option<Arc<AddonBackendSnapshot>>,
    pub scene: Option<Arc<SceneSnapshot>>,
    pub revision: u64,
    pub from_cache: bool,
    pub filtered: bool,
    pub error: Option<Error>,
    epoch: u64,
    raw_version: u64,
}
impl SceneView {
    pub fn activation(&self) -> u64 {
        self.epoch
    }
}
#[derive(Default)]
struct ProfileScene {
    scene: Option<Arc<SceneSnapshot>>,
    version: u64,
    restored: bool,
    from_cache: bool,
    error: Option<Error>,
}
struct State {
    active: Option<Arc<AddonBackendSnapshot>>,
    epoch: u64,
    sequence: u64,
    profiles: HashMap<ContentScope, ProfileScene>,
    published: Arc<SceneView>,
    clearing: usize,
}
struct Inner {
    source: Arc<dyn SceneSource>,
    storage: Arc<dyn SceneStorage>,
    state: Mutex<State>,
}
#[derive(Clone)]
pub struct SceneBackend(Arc<Inner>);
fn same_selection(left: &AddonSnapshot, right: &AddonSnapshot) -> bool {
    left.context == right.context
        && left.list_revision == right.list_revision
        && left.authoritative == right.authoritative
        && left
            .records
            .iter()
            .map(|record| &record.installed)
            .eq(right.records.iter().map(|record| &record.installed))
}
fn same_manifests(left: &AddonSnapshot, right: &AddonSnapshot) -> bool {
    left.records
        .iter()
        .map(|record| &record.manifest)
        .eq(right.records.iter().map(|record| &record.manifest))
}
fn scope(source: &AddonBackendSnapshot) -> ContentScope {
    ContentScope::from(
        source
            .addons
            .context
            .as_ref()
            .expect("verified scene context"),
    )
}
/// Reuse only currently configured, enabled catalog sources. Unknown manifests
/// preserve useful rows; a confirmed removed declaration cannot resurrect one.
fn visible(scene: &Arc<SceneSnapshot>, source: &AddonSnapshot) -> Result<Arc<SceneSnapshot>> {
    let keep = scene
        .rows
        .iter()
        .map(|row| {
            let Some(request) = &row.record.source else {
                return true;
            };
            source.records.iter().any(|record| {
                record.installed.enabled
                    && record.installed.url == request.addon
                    && record.manifest.as_ref().is_none_or(|manifest| {
                        manifest.catalogs.iter().any(|catalog| {
                            catalog.id == request.catalog_id
                                && catalog.media_type == request.media_type
                                && catalog.extra.iter().filter(|extra| extra.required).all(
                                    |extra| {
                                        request.extras.iter().any(|(name, value)| {
                                            name == &extra.name && !value.is_empty()
                                        })
                                    },
                                )
                        })
                    })
            })
        })
        .collect::<Vec<_>>();
    if keep.iter().all(|keep| *keep) {
        return Ok(scene.clone());
    }
    let mut covered = vec![false; scene.titles.len()];
    let mut retained = vec![false; scene.titles.len()];
    for (row, keep) in scene.rows.iter().zip(&keep) {
        for index in row.record.titles.clone() {
            covered[index] = true;
            retained[index] |= *keep;
        }
    }
    let mut offsets = Vec::with_capacity(scene.titles.len() + 1);
    offsets.push(0);
    let mut titles = Vec::new();
    for (index, title) in scene.titles.iter().enumerate() {
        if retained[index]
            || !covered[index]
            || title.record.on_watchlist
            || title.record.in_collection
            || title.record.resume.percentage > 0
        {
            titles.push(title.clone());
        }
        offsets.push(titles.len());
    }
    let mut rows = Vec::new();
    for (row, keep) in scene.rows.iter().zip(keep) {
        if !keep {
            continue;
        }
        let mut record = row.record.clone();
        record.titles = offsets[record.titles.start]..offsets[record.titles.end];
        rows.push(Arc::new(RowSnapshot::new(record)?));
    }
    Ok(Arc::new(SceneSnapshot { titles, rows }))
}
impl SceneBackend {
    pub fn home_plan_keys(&self, work: &SceneWork) -> Result<Vec<String>> {
        self.0
            .storage
            .home_plan_keys(&work.scope(), &|| self.current(work))
    }
    pub fn save_home_plan(&self, work: &SceneWork, keys: &[String]) -> Result<()> {
        self.0
            .storage
            .save_home_plan(&work.scope(), keys, &|| self.current(work))
    }
    pub fn new(source: Arc<dyn SceneSource>, storage: Arc<dyn SceneStorage>) -> Self {
        let published = Arc::new(SceneView {
            source: None,
            scene: None,
            revision: 0,
            from_cache: false,
            filtered: false,
            error: None,
            epoch: 0,
            raw_version: 0,
        });
        Self(Arc::new(Inner {
            source,
            storage,
            state: Mutex::new(State {
                active: None,
                epoch: 0,
                sequence: 0,
                profiles: HashMap::new(),
                published,
                clearing: 0,
            }),
        }))
    }
    fn state(&self) -> MutexGuard<'_, State> {
        self.0
            .state
            .lock()
            .unwrap_or_else(|error| error.into_inner())
    }
    fn publish_view(&self, state: &mut State) -> Result<()> {
        let profile = state
            .active
            .as_ref()
            .and_then(|source| state.profiles.get(&scope(source)));
        let raw_version = profile.map(|profile| profile.version).unwrap_or(0);
        let same_source = match (&state.active, &state.published.source) {
            (Some(active), Some(previous)) => {
                same_selection(&active.addons, &previous.addons)
                    && same_manifests(&active.addons, &previous.addons)
            }
            (None, None) => true,
            _ => false,
        };
        let scene = if raw_version == state.published.raw_version && same_source {
            state.published.scene.clone()
        } else {
            match (
                profile.and_then(|profile| profile.scene.as_ref()),
                state.active.as_ref(),
            ) {
                (Some(scene), Some(source)) => Some(visible(scene, &source.addons)?),
                _ => None,
            }
        };
        let filtered = match (
            profile.and_then(|profile| profile.scene.as_ref()),
            scene.as_ref(),
        ) {
            (Some(raw), Some(visible)) => !Arc::ptr_eq(raw, visible),
            _ => false,
        };
        state.published = Arc::new(SceneView {
            source: state.active.clone(),
            scene,
            revision: state.published.revision.wrapping_add(1),
            from_cache: profile.is_some_and(|profile| profile.from_cache),
            filtered,
            error: profile.and_then(|profile| profile.error.clone()),
            epoch: state.epoch,
            raw_version,
        });
        Ok(())
    }
    /// Called at frame/action boundaries. No disk or network work occurs here.
    pub fn activate(&self) -> Result<()> {
        let snapshot = self.0.source.snapshot();
        let ready = snapshot.addons.authoritative
            && snapshot.addons.context.as_ref().is_some_and(|context| {
                !context.addons.member.trim().is_empty()
                    && !context.addons.owner.trim().is_empty()
                    && !context.addons.member.contains('\0')
                    && !context.addons.owner.contains('\0')
            })
            && self.0.source.current(&snapshot.addons);
        let mut active = ready.then_some(snapshot);
        let mut state = self.state();
        if state.clearing != 0 {
            active = None;
        }
        let same = match (&state.active, &active) {
            (Some(old), Some(new)) => same_selection(&old.addons, &new.addons),
            (None, None) => true,
            _ => false,
        };
        let metadata_changed = match (&state.active, &active) {
            (Some(old), Some(new)) => !same_manifests(&old.addons, &new.addons),
            _ => false,
        };
        if !same {
            state.epoch = state.epoch.wrapping_add(1);
            state.sequence = 0;
        }
        state.active = active;
        if let Some(source) = state.active.as_ref() {
            let key = scope(source);
            state.profiles.entry(key).or_default();
        }
        if !same || metadata_changed {
            self.publish_view(&mut state)?;
        }
        Ok(())
    }
    pub fn snapshot(&self) -> Arc<SceneView> {
        self.state().published.clone()
    }
    pub fn snapshot_current(&self, view: &SceneView) -> bool {
        let state = self.state();
        state.epoch == view.epoch
            && state.published.revision == view.revision
            && view
                .source
                .as_ref()
                .is_none_or(|source| self.0.source.current(&source.addons))
    }
    pub fn context_current(&self, view: &SceneView) -> bool {
        let state = self.state();
        state.epoch == view.epoch
            && view
                .source
                .as_ref()
                .is_some_and(|source| self.0.source.current(&source.addons))
    }
    fn work(&self, kind: WorkKind) -> Result<SceneWork> {
        let mut state = self.state();
        let source = state.active.clone().ok_or(Error::Cancelled)?;
        if !self.0.source.current(&source.addons) {
            return Err(Error::Cancelled);
        }
        if kind == WorkKind::Publish {
            state.sequence = state.sequence.wrapping_add(1);
        }
        Ok(SceneWork {
            version: state
                .profiles
                .get(&scope(&source))
                .expect("active scene profile")
                .version,
            source,
            epoch: state.epoch,
            sequence: state.sequence,
            kind,
        })
    }
    pub fn begin_restore(&self) -> Result<SceneWork> {
        self.work(WorkKind::Restore)
    }
    pub fn begin_publish(&self) -> Result<SceneWork> {
        self.work(WorkKind::Publish)
    }
    fn current_locked(&self, state: &State, work: &SceneWork) -> bool {
        state.epoch == work.epoch
            && state
                .active
                .as_ref()
                .is_some_and(|source| same_selection(&source.addons, &work.source.addons))
            && self.0.source.current(&work.source.addons)
            && match work.kind {
                WorkKind::Restore => state
                    .profiles
                    .get(&work.scope())
                    .is_some_and(|profile| profile.version == work.version),
                WorkKind::Publish => state.sequence == work.sequence,
            }
    }
    pub fn current(&self, work: &SceneWork) -> bool {
        self.current_locked(&self.state(), work)
    }
    pub fn needs_restore(&self) -> bool {
        let state = self.state();
        state.active.as_ref().is_some_and(|source| {
            !state
                .profiles
                .get(&scope(source))
                .expect("active scene profile")
                .restored
        })
    }
    /// Restoration cannot replace a scene accepted while its file was read.
    pub fn restore(&self, work: &SceneWork) -> Result<()> {
        if work.kind != WorkKind::Restore || !self.current(work) {
            return Err(Error::Cancelled);
        }
        let loaded = self.0.storage.load(&work.scope(), &|| self.current(work));
        let mut state = self.state();
        if !self.current_locked(&state, work) {
            return Err(Error::Cancelled);
        }
        let profile = state
            .profiles
            .get_mut(&work.scope())
            .expect("active scene profile");
        match loaded {
            Ok(scene) => {
                if profile.scene.is_none() {
                    profile.scene = scene;
                    profile.from_cache = profile.scene.is_some();
                    profile.version = profile.version.wrapping_add(1);
                }
                profile.restored = true;
                profile.error = None;
            }
            Err(error) => {
                profile.error = Some(error.clone());
                self.publish_view(&mut state)?;
                return Err(error);
            }
        }
        self.publish_view(&mut state)
    }
    /// Accept display data before saving. Storage failure cannot erase fresh data.
    pub fn accept(&self, work: &SceneWork, scene: Arc<SceneSnapshot>) -> Result<()> {
        if work.kind != WorkKind::Publish {
            return Err(Error::Cancelled);
        }
        scene.validate()?;
        let mut state = self.state();
        if !self.current_locked(&state, work) {
            return Err(Error::Cancelled);
        }
        let profile = state
            .profiles
            .get_mut(&work.scope())
            .expect("active scene profile");
        profile.scene = Some(scene);
        profile.version = profile.version.wrapping_add(1);
        profile.restored = true;
        profile.from_cache = false;
        profile.error = None;
        self.publish_view(&mut state)
    }
    pub fn save(&self, work: &SceneWork, scene: &SceneSnapshot) -> Result<()> {
        if work.kind != WorkKind::Publish || !self.current(work) {
            return Err(Error::Cancelled);
        }
        let version = {
            let state = self.state();
            if !self.current_locked(&state, work) {
                return Err(Error::Cancelled);
            }
            let profile = state.profiles.get(&work.scope()).ok_or(Error::Cancelled)?;
            if !profile
                .scene
                .as_deref()
                .is_some_and(|current| std::ptr::eq(current, scene))
            {
                return Err(Error::Cancelled);
            }
            profile.version
        };
        let still_current = || {
            let state = self.state();
            self.current_locked(&state, work)
                && state
                    .profiles
                    .get(&work.scope())
                    .is_some_and(|profile| profile.version == version)
        };
        let saved = self.0.storage.save(&work.scope(), scene, &still_current);
        let mut state = self.state();
        if !self.current_locked(&state, work)
            || state
                .profiles
                .get(&work.scope())
                .is_none_or(|profile| profile.version != version)
        {
            return Err(Error::Cancelled);
        }
        let profile = state
            .profiles
            .get_mut(&work.scope())
            .expect("active scene profile");
        let error = saved.as_ref().err().cloned();
        if profile.error != error {
            profile.error = error;
            self.publish_view(&mut state)?;
        }
        saved
    }
    pub fn publish(&self, work: &SceneWork, scene: CatalogScene) -> Result<()> {
        let scene = Arc::new(SceneSnapshot::new(scene)?);
        self.accept(work, scene.clone())?;
        self.save(work, &scene)
    }
    pub fn clear(&self) -> Result<()> {
        {
            let mut state = self.state();
            state.epoch = state.epoch.wrapping_add(1);
            state.active = None;
            state.clearing += 1;
            state.profiles.clear();
            self.publish_view(&mut state)?;
        }
        let cleared = self.0.storage.clear();
        self.state().clearing -= 1;
        cleared
    }
}
