//! Runtime catalog requests and background restoration for the retained UI.
use crate::{
    addon_backend::AddonBackend,
    addon_service::AddonAccount,
    addon_snapshot::AddonSnapshot,
    addons::{AddonUrl, InstalledAddon},
    catalog_cache::{ProviderCache, ProviderResource},
    catalog_scene::{CatalogRow, CatalogRowState, CatalogScene, CatalogSource, SceneTitle},
    content_context::ContentContext,
    error::{Error, Result},
    provider_http::ProviderTransport,
    provider_loader::ProviderLoader,
    scene_backend::{SceneBackend, SceneStorage, SceneWork},
    scene_snapshot::SceneSnapshot,
};
use std::{
    sync::{Arc, Condvar, Mutex},
    thread::{self, JoinHandle},
    time::{Duration, Instant},
};

struct RequestAccount {
    account: Arc<dyn AddonAccount>,
    addons: AddonBackend,
    source: AddonSnapshot,
}
impl AddonAccount for RequestAccount {
    fn current_context(&self, force_primary: bool) -> Option<ContentContext> {
        self.account
            .current_context(force_primary)
            .filter(|context| self.is_current(context))
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.source.context.as_ref() == Some(context)
            && self.addons.snapshot_current(&self.source)
            && self.account.is_current(context)
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        if !self.is_current(context) {
            return Err(Error::Cancelled);
        }
        self.account.read_addons(context)
    }
    fn write_addons(&self, context: &ContentContext, addons: &[InstalledAddon]) -> Result<()> {
        if !self.is_current(context) {
            return Err(Error::Cancelled);
        }
        self.account.write_addons(context, addons)
    }
}
struct Queue {
    pending: Option<SceneWork>,
    selected: Option<SceneWork>,
    clear: bool,
    stopped: bool,
    retry: Instant,
}
struct Worker {
    queue: Mutex<Queue>,
    ready: Condvar,
}
pub struct CatalogRuntime {
    pub scenes: SceneBackend,
    addons: AddonBackend,
    account: Arc<dyn AddonAccount>,
    transport: Arc<dyn ProviderTransport>,
    cache: Arc<dyn ProviderCache>,
    worker: Arc<Worker>,
    _thread: JoinHandle<()>,
}
pub struct CatalogResult {
    pub scene: SceneSnapshot,
    pub responded: bool,
}
impl CatalogRuntime {
    pub fn content_context(&self) -> Option<ContentContext> {
        self.account.current_context(false)
    }
    pub fn content_current(&self, context: &ContentContext) -> bool {
        self.account.is_current(context)
    }
    pub fn context_addons(&self, view: &crate::scene_backend::SceneView) -> Option<AddonSnapshot> {
        if !self.scenes.context_current(view) {
            return None;
        }
        let source = self.addons.snapshot();
        self.scenes
            .context_current(view)
            .then(|| source.addons.clone())
    }
    pub fn row_reusable(
        &self,
        work: &SceneWork,
        base: &AddonUrl,
        media_type: &str,
        id: &str,
    ) -> bool {
        if !self.scenes.current(work) {
            return false;
        }
        self.addons.snapshot().addons.records.iter().any(|record| {
            record.installed.enabled
                && &record.installed.url == base
                && record.manifest.as_ref().is_none_or(|manifest| {
                    manifest.catalogs.iter().any(|catalog| {
                        catalog.id == id
                            && catalog.media_type == media_type
                            && catalog.extra.iter().all(|extra| !extra.required)
                            && catalog.extra_required.is_empty()
                    })
                })
        })
    }
    pub fn describe(
        &self,
        work: &SceneWork,
        addon_index: usize,
        catalog_index: usize,
    ) -> Result<(String, String, String, String)> {
        if !self.scenes.current(work) {
            return Err(Error::Cancelled);
        }
        let source = self.addons.snapshot();
        let record = source
            .addons
            .records
            .get(addon_index)
            .ok_or(Error::InvalidData)?;
        let manifest = record.manifest.as_ref().ok_or(Error::InvalidData)?;
        let catalog = manifest
            .catalogs
            .get(catalog_index)
            .ok_or(Error::InvalidData)?;
        Ok((
            record.installed.url.as_str().into(),
            catalog.media_type.clone(),
            catalog.id.clone(),
            manifest.id.clone(),
        ))
    }
    pub fn new(
        account: Arc<dyn AddonAccount>,
        addons: AddonBackend,
        transport: Arc<dyn ProviderTransport>,
        cache: Arc<dyn ProviderCache>,
        storage: Arc<dyn SceneStorage>,
    ) -> Result<Self> {
        let scenes = SceneBackend::new(Arc::new(addons.clone()), storage);
        let worker = Arc::new(Worker {
            queue: Mutex::new(Queue {
                pending: None,
                selected: None,
                clear: false,
                stopped: false,
                retry: Instant::now(),
            }),
            ready: Condvar::new(),
        });
        let shared = worker.clone();
        let backend = scenes.clone();
        let provider_cache = cache.clone();
        let thread = thread::Builder::new()
            .name("nuvio-scene-cache".into())
            .spawn(move || loop {
                let (work, clear, stopped) = {
                    let mut queue = shared.queue.lock().unwrap_or_else(|e| e.into_inner());
                    while queue.pending.is_none() && !queue.clear && !queue.stopped {
                        queue = shared.ready.wait(queue).unwrap_or_else(|e| e.into_inner());
                    }
                    let clear = queue.clear;
                    queue.clear = false;
                    (queue.pending.take(), clear, queue.stopped)
                };
                if clear {
                    let _ = backend.clear();
                    let _ = provider_cache.clear();
                }
                if stopped {
                    break;
                }
                if let Some(work) = work {
                    let _ = backend.restore(&work);
                }
            })
            .map_err(|_| Error::Network)?;
        Ok(Self {
            scenes,
            addons,
            account,
            transport,
            cache,
            worker,
            _thread: thread,
        })
    }
    /// Frame path: selects memory and schedules I/O without waiting for it.
    pub fn step(&self) -> Result<()> {
        self.scenes.activate()?;
        if !self.scenes.needs_restore() {
            return Ok(());
        }
        let mut queue = self.worker.queue.lock().unwrap_or_else(|e| e.into_inner());
        let changed = queue
            .selected
            .as_ref()
            .is_none_or(|work| !self.scenes.current(work));
        if changed || Instant::now() >= queue.retry {
            let work = self.scenes.begin_restore()?;
            queue.selected = Some(work.clone());
            queue.pending = Some(work);
            queue.retry = Instant::now() + Duration::from_secs(30);
            self.worker.ready.notify_one();
        }
        Ok(())
    }
    pub fn logout(&self) {
        let _ = self.scenes.activate();
        let mut queue = self.worker.queue.lock().unwrap_or_else(|e| e.into_inner());
        queue.selected = None;
        queue.pending = None;
        queue.clear = true;
        self.worker.ready.notify_one();
    }
    pub fn load(
        &self,
        work: &SceneWork,
        base: &str,
        media_type: &str,
        id: &str,
        origin: &str,
        limit: usize,
    ) -> Result<CatalogResult> {
        if !self.scenes.current(work) {
            return Err(Error::Cancelled);
        }
        let addon = AddonUrl::parse(base)?;
        if !work
            .source
            .addons
            .records
            .iter()
            .any(|record| record.installed.enabled && record.installed.url == addon)
        {
            return Err(Error::Cancelled);
        }
        let context = work
            .source
            .addons
            .context
            .as_ref()
            .ok_or(Error::Cancelled)?;
        let account = Arc::new(RequestAccount {
            account: self.account.clone(),
            addons: self.addons.clone(),
            source: work.source.addons.clone(),
        });
        let loader = ProviderLoader::new(account, self.transport.clone(), self.cache.clone());
        let result = loader.catalog(
            context,
            &addon,
            ProviderResource::Catalog {
                media_type: media_type.into(),
                id: id.into(),
                extras: Vec::new(),
            },
            Some(limit),
        )?;
        if !self.scenes.current(work) {
            return Err(Error::Cancelled);
        }
        let source = self.addons.snapshot();
        let manifest = source
            .addons
            .records
            .iter()
            .find(|record| record.installed.url == addon)
            .and_then(|record| record.manifest.as_ref());
        let catalog = manifest.and_then(|manifest| {
            manifest
                .catalogs
                .iter()
                .find(|catalog| catalog.id == id && catalog.media_type == media_type)
        });
        let provider = manifest
            .map(|manifest| manifest.id.as_str())
            .filter(|id| !id.is_empty())
            .unwrap_or(origin);
        let raw_name = catalog
            .map(|catalog| catalog.name.as_str())
            .filter(|name| !name.is_empty())
            .unwrap_or(id);
        let label = match media_type {
            "series" => "Series",
            "channel" | "channels" | "tv" => "Channels",
            _ => "Movie",
        };
        let mut name = if raw_name.to_lowercase().ends_with(&label.to_lowercase()) {
            raw_name.into()
        } else {
            format!("{raw_name} - {label}")
        };
        if name.as_bytes().first().is_some_and(u8::is_ascii_lowercase) {
            name.get_mut(..1).unwrap().make_ascii_uppercase();
        }
        let titles: Vec<_> = result
            .value
            .titles
            .into_iter()
            .map(|metadata| {
                let mut title = SceneTitle::from(metadata);
                title.origin = provider.into();
                title
            })
            .collect();
        let count = titles.len();
        Ok(CatalogResult {
            scene: SceneSnapshot::new(CatalogScene {
                titles,
                rows: vec![CatalogRow {
                    key: format!("{provider}_{media_type}_{id}"),
                    name,
                    media_type: media_type.into(),
                    source: Some(CatalogSource {
                        addon,
                        media_type: media_type.into(),
                        catalog_id: id.into(),
                        extras: Vec::new(),
                    }),
                    titles: 0..count,
                    state: if count == 0 {
                        CatalogRowState::ConfirmedEmpty
                    } else {
                        CatalogRowState::Ready
                    },
                }],
            })?,
            responded: result.request_error.is_none(),
        })
    }
}
impl Drop for CatalogRuntime {
    fn drop(&mut self) {
        let mut queue = self.worker.queue.lock().unwrap_or_else(|e| e.into_inner());
        queue.stopped = true;
        queue.pending = None;
        self.worker.ready.notify_one();
    }
}
