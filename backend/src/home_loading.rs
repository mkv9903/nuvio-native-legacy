//! Home catalog planning, bounded loading and ordered scene assembly.
use crate::{
    addon_snapshot::AddonSnapshot,
    catalog_order::HomeOrder,
    catalog_runtime::CatalogResult,
    catalog_scene::CatalogRowState,
    error::{Error, Result},
    scene_snapshot::{RowSnapshot, SceneSnapshot},
};
use std::{
    collections::{HashMap, HashSet, VecDeque},
    sync::{
        atomic::{AtomicBool, AtomicUsize, Ordering},
        mpsc, Arc,
    },
};

pub const CATALOG_WORKERS: usize = 3;
pub const MANIFEST_WORKERS: usize = 4;

#[derive(Clone, Default)]
pub struct RowChoice {
    pub local_position: Option<usize>,
    pub hidden: bool,
    pub represented_by_collection: bool,
    pub channel_selected: bool,
}
#[derive(Clone)]
pub struct HomeCatalog {
    pub addon_index: usize,
    pub catalog_index: usize,
    pub key: String,
    pub name: String,
    pub raw_name: String,
    pub media_type: String,
    pub id: String,
    pub origin: String,
    seed_row: Option<usize>,
    pub searchable: bool,
    pub required_extras: bool,
    pub channel: bool,
    pub choice: RowChoice,
}
pub struct HomePlan {
    pub source: AddonSnapshot,
    pub catalogs: Vec<HomeCatalog>,
    pub account_order: Arc<HomeOrder>,
    pub previous: Option<Arc<SceneSnapshot>>,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum UpdateKind {
    Partial = 0,
    Complete = 1,
    Progress = 2,
}
#[derive(Clone, Copy, Default)]
pub struct LoadStats {
    pub requested: usize,
    pub responded: usize,
    pub empty: usize,
    pub failed: usize,
    pub rows: usize,
    pub remaining: usize,
}
pub struct HomeLoaded {
    pub scene: Arc<SceneSnapshot>,
    pub stats: LoadStats,
}

pub fn row_name(raw: &str, media_type: &str) -> String {
    let label = match media_type {
        "series" => "Series",
        "channel" | "channels" | "tv" | "live" | "iptv" => "Channels",
        _ => "Movie",
    };
    let mut name = if raw.is_empty() {
        label.into()
    } else if raw.to_lowercase().ends_with(&label.to_lowercase()) {
        raw.into()
    } else {
        format!("{raw} - {label}")
    };
    if name.as_bytes().first().is_some_and(u8::is_ascii_lowercase) {
        name.get_mut(..1).unwrap().make_ascii_uppercase();
    }
    name
}

impl HomePlan {
    pub fn new(
        source: AddonSnapshot,
        account_order: Arc<HomeOrder>,
        previous: Option<Arc<SceneSnapshot>>,
    ) -> Self {
        let mut catalogs = Vec::new();
        for (addon_index, record) in source.records.iter().enumerate() {
            let Some(manifest) = &record.manifest else {
                continue;
            };
            for (catalog_index, catalog) in manifest.catalogs.iter().enumerate() {
                let raw_name = if catalog.name.is_empty() {
                    &catalog.id
                } else {
                    &catalog.name
                };
                catalogs.push(HomeCatalog {
                    addon_index,
                    catalog_index,
                    key: format!("{}_{}_{}", manifest.id, catalog.media_type, catalog.id),
                    name: row_name(raw_name, &catalog.media_type),
                    raw_name: catalog.name.clone(),
                    media_type: catalog.media_type.clone(),
                    id: catalog.id.clone(),
                    origin: manifest.id.clone(),
                    seed_row: None,
                    searchable: catalog.searchable(),
                    required_extras: catalog.extra.iter().any(|extra| extra.required)
                        || !catalog.extra_required.is_empty(),
                    channel: catalog.is_channel(),
                    choice: RowChoice::default(),
                });
            }
        }
        if let Some(previous) = &previous {
            for (row_index, row) in previous.rows.iter().enumerate() {
                let Some(request) = &row.record.source else {
                    continue;
                };
                let Some(addon_index) = source.records.iter().position(|record| {
                    record.installed.url == request.addon && record.manifest.is_none()
                }) else {
                    continue;
                };
                catalogs.push(HomeCatalog {
                    addon_index,
                    catalog_index: usize::MAX,
                    key: row.record.key.clone(),
                    name: row.record.name.clone(),
                    raw_name: row.record.name.clone(),
                    media_type: request.media_type.clone(),
                    id: request.catalog_id.clone(),
                    origin: previous
                        .titles
                        .get(row.record.titles.start)
                        .map(|title| title.record.origin.clone())
                        .unwrap_or_default(),
                    seed_row: Some(row_index),
                    searchable: false,
                    required_extras: false,
                    channel: matches!(
                        request.media_type.as_str(),
                        "channel" | "channels" | "tv" | "live" | "iptv"
                    ),
                    choice: RowChoice::default(),
                });
            }
        }
        Self {
            source,
            catalogs,
            account_order,
            previous,
        }
    }
    pub fn disable_key(&self, index: usize) -> String {
        let entry = &self.catalogs[index];
        let record = &self.source.records[entry.addon_index];
        format!(
            "{}_{}_{}_{}",
            record.installed.url.as_str(),
            entry.media_type,
            entry.id,
            entry.raw_name
        )
    }
    pub fn eligible(&self, index: usize) -> bool {
        let entry = &self.catalogs[index];
        self.source.records[entry.addon_index].installed.enabled
            && !entry.required_extras
            && !entry.choice.hidden
            && !entry.choice.represented_by_collection
            && (!entry.channel || entry.choice.channel_selected)
            && !self.account_order.hidden.contains(&entry.key)
            && !self.account_order.hidden.contains(&self.disable_key(index))
    }
    /// Explicit local and account order win; accepted rows precede new rows.
    /// No new-addon promotion, fixed declaration quota or provider count exists.
    pub fn order(&self) -> Vec<usize> {
        let mut groups: Vec<VecDeque<usize>> = vec![VecDeque::new(); self.source.records.len()];
        let mut seen = HashSet::new();
        for (index, catalog) in self.catalogs.iter().enumerate() {
            if self.eligible(index) && seen.insert(catalog.key.as_str()) {
                groups[catalog.addon_index].push_back(index);
            }
        }
        let mut order = Vec::with_capacity(seen.len());
        loop {
            let before = order.len();
            for group in &mut groups {
                if let Some(index) = group.pop_front() {
                    order.push(index);
                }
            }
            if order.len() == before {
                break;
            }
        }
        let account: HashMap<_, _> = self
            .account_order
            .keys
            .iter()
            .enumerate()
            .map(|(index, key)| (key.as_str(), index))
            .collect();
        let previous: HashMap<_, _> = self
            .previous
            .as_ref()
            .map(|scene| {
                scene
                    .rows
                    .iter()
                    .enumerate()
                    .map(|(index, row)| (row.record.key.as_str(), index))
                    .collect()
            })
            .unwrap_or_default();
        order.sort_by_key(|index| {
            let entry = &self.catalogs[*index];
            (
                entry.choice.local_position.unwrap_or(usize::MAX),
                account
                    .get(entry.key.as_str())
                    .copied()
                    .unwrap_or(usize::MAX),
                previous
                    .get(entry.key.as_str())
                    .copied()
                    .unwrap_or(usize::MAX),
            )
        });
        order
    }
    pub fn previous_row(&self, index: usize) -> Option<SceneSnapshot> {
        let previous = self.previous.as_ref()?;
        let entry = &self.catalogs[index];
        let record = &self.source.records[entry.addon_index];
        let row = previous.rows.iter().find(|row| {
            row.record.key == entry.key
                && row
                    .record
                    .source
                    .as_ref()
                    .is_some_and(|source| source.addon == record.installed.url)
                && row.record.state == CatalogRowState::Ready
                && !row.record.titles.is_empty()
        })?;
        let mut row_record = row.record.clone();
        let titles = previous.titles.get(row_record.titles.clone())?.to_vec();
        row_record.titles = 0..titles.len();
        Some(SceneSnapshot {
            titles,
            rows: vec![Arc::new(RowSnapshot::new(row_record).ok()?)],
        })
    }

    fn assemble(
        &self,
        order: &[usize],
        loaded: &HashMap<usize, Arc<SceneSnapshot>>,
    ) -> Result<Arc<SceneSnapshot>> {
        let mut result = SceneSnapshot {
            titles: Vec::new(),
            rows: Vec::new(),
        };
        for index in order {
            let Some(scene) = loaded.get(index) else {
                continue;
            };
            let offset = result.titles.len();
            result.titles.extend(scene.titles.iter().cloned());
            for row in &scene.rows {
                let mut row = row.record.clone();
                row.titles = row.titles.start + offset..row.titles.end + offset;
                result.rows.push(Arc::new(RowSnapshot::new(row)?));
            }
        }
        result.validate()?;
        Ok(Arc::new(result))
    }
    /// The calling worker receives progressive snapshots. Network workers never
    /// call the UI, and a cancelled callback stops further queued requests.
    pub fn load(
        &self,
        limit: usize,
        fetch: &(impl Fn(usize) -> Result<CatalogResult> + Sync),
        current: &(impl Fn() -> bool + Sync),
        mut publish: impl FnMut(&Arc<SceneSnapshot>, UpdateKind, LoadStats) -> bool,
    ) -> Result<HomeLoaded> {
        let order = self.order();
        let warm =
            self.previous.as_ref().is_some_and(|scene| {
                scene.rows.iter().any(|row| {
                    row.record.key != "continue_watching" && !row.record.titles.is_empty()
                }) || scene
                    .titles
                    .iter()
                    .any(|title| title.record.on_watchlist || title.record.in_collection)
            });
        let mut stats = LoadStats::default();
        let mut loaded = HashMap::new();
        let mut cursor = 0;
        while loaded.len() < limit && cursor < order.len() {
            if !current() {
                return Err(Error::Cancelled);
            }
            let end = (cursor + limit - loaded.len()).min(order.len());
            let batch = &order[cursor..end];
            let next = AtomicUsize::new(0);
            let cancelled = AtomicBool::new(false);
            std::thread::scope(|scope| {
                let (tx, rx) = mpsc::sync_channel(CATALOG_WORKERS);
                let mut started = 0;
                for _ in 0..CATALOG_WORKERS.min(batch.len()) {
                    let tx = tx.clone();
                    let next = &next;
                    let cancelled = &cancelled;
                    if std::thread::Builder::new()
                        .name("nuvio-home-catalog".into())
                        .spawn_scoped(scope, move || loop {
                            if cancelled.load(Ordering::Acquire) || !current() {
                                break;
                            }
                            let slot = next.fetch_add(1, Ordering::Relaxed);
                            let Some(index) = batch.get(slot) else {
                                break;
                            };
                            let result = if self.catalogs[*index].seed_row.is_some() {
                                self.previous_row(*index)
                                    .map(|scene| CatalogResult {
                                        scene,
                                        responded: false,
                                    })
                                    .ok_or(Error::InvalidData)
                            } else {
                                fetch(*index)
                            };
                            if tx.send((*index, result)).is_err() {
                                break;
                            }
                        })
                        .is_ok()
                    {
                        started += 1;
                    }
                }
                drop(tx);
                if started == 0 {
                    return Err(Error::Network);
                }
                for (index, result) in rx {
                    stats.requested += 1;
                    if cancelled.load(Ordering::Acquire) || !current() {
                        cancelled.store(true, Ordering::Release);
                        continue;
                    }
                    let scene = match result {
                        Ok(result) => {
                            if result.responded {
                                stats.responded += 1;
                            } else {
                                stats.failed += 1;
                            }
                            if result.scene.titles.is_empty() {
                                stats.empty += 1;
                                None
                            } else {
                                Some(result.scene)
                            }
                        }
                        Err(Error::Cancelled) => {
                            cancelled.store(true, Ordering::Release);
                            None
                        }
                        Err(_) => {
                            stats.failed += 1;
                            self.previous_row(index)
                        }
                    };
                    if let Some(scene) = scene {
                        loaded.insert(index, Arc::new(scene));
                        stats.rows = loaded.len();
                        let snapshot = self.assemble(&order, &loaded)?;
                        if !publish(
                            &snapshot,
                            if warm {
                                UpdateKind::Progress
                            } else {
                                UpdateKind::Partial
                            },
                            stats,
                        ) {
                            cancelled.store(true, Ordering::Release);
                        }
                    }
                }
                if cancelled.load(Ordering::Acquire) {
                    Err(Error::Cancelled)
                } else {
                    Ok(())
                }
            })?;
            cursor = end;
        }
        if !current() {
            return Err(Error::Cancelled);
        }
        stats.remaining = order.len() - cursor;
        let scene = self.assemble(&order, &loaded)?;
        if !publish(&scene, UpdateKind::Complete, stats) {
            return Err(Error::Cancelled);
        }
        Ok(HomeLoaded { scene, stats })
    }
}
