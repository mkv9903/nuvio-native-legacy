use super::workers::Workers;
use crate::{
    account::{CloudClient, DeviceLogin, Session},
    catalog::{
        self, AccountProfiles, Addon, CatalogService, HomeOrder, HomeRow, InstalledAddon,
        SourceChoice, Title,
    },
    error::{AppError, Result},
    http::Transport,
    profile::{HomeTicket, ProfileHome},
    secret::Secret,
};
use serde_json::json;
use std::{
    collections::HashSet,
    sync::{
        atomic::{AtomicU64, Ordering},
        mpsc::Receiver,
        Arc,
    },
    time::{Duration, Instant},
};
use zeroize::Zeroizing;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Page {
    Login,
    Profiles,
    Home,
    Detail,
    Sources,
    Search,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Input {
    Up,
    Down,
    Left,
    Right,
    Accept,
    Back,
    Search,
    Profiles,
    Delete,
    Confirm,
}
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EditPurpose {
    Email,
    Password,
    Search,
    Pin,
}

pub const KEYS: &[&str] = &[
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "1", "2", "a", "s", "d", "f", "g", "h", "j",
    "k", "l", "@", "3", "4", "z", "x", "c", "v", "b", "n", "m", ".", "-", "_", "5", "6", "0", "7",
    "8", "9", "!", "#", "$", "%", "&", "*", "(", ")", ":", "/", "?", "=", "+", "\"", "'", "\\",
    " ", "Shift", "Delete", "Done",
];

pub struct Editor {
    pub purpose: EditPurpose,
    pub value: Zeroizing<String>,
    pub focus: usize,
    pub shift: bool,
}
pub struct PlaybackSelection {
    pub title: Title,
    pub video_id: String,
    pub choice: SourceChoice,
}

enum Reply {
    Login(u64, u64, Result<Session>),
    Device(u64, Result<DeviceLogin>),
    Poll(u64, u64, Result<Option<Session>>),
    Profiles(u64, Result<AccountProfiles>),
    Seed(u64, HomeTicket, Result<(Vec<InstalledAddon>, HomeOrder)>),
    Manifest(u64, HomeTicket, Result<Addon>),
    Row(u64, HomeTicket, String, usize, Result<Vec<Title>>),
    Detail(u64, u64, Result<Title>),
    Sources(u64, u64, Result<Vec<SourceChoice>>),
    Search(u64, u64, Result<Vec<Title>>),
    Pin(u64, usize, Result<()>),
}

pub struct App {
    // Drop events before workers, allowing blocked senders to finish on shutdown.
    events: Receiver<Reply>,
    workers: Workers<Reply>,
    cloud: Arc<CloudClient>,
    transport: Arc<dyn Transport>,
    epoch: Arc<AtomicU64>,
    pub page: Page,
    pub account: Option<AccountProfiles>,
    pub active_profile: Option<usize>,
    pub home: ProfileHome<HomeRow>,
    pub addons: Vec<Addon>,
    order: HomeOrder,
    manifest_queue: Vec<InstalledAddon>,
    addon_order: std::collections::HashMap<String, usize>,
    manifest_loading: usize,
    pub profile_focus: usize,
    pub row_focus: usize,
    pub card_focus: usize,
    pub rail_focus: Option<usize>,
    pub login_focus: usize,
    pub email: String,
    pub password: Secret,
    pub login_code: Option<Arc<DeviceLogin>>,
    next_poll: Instant,
    poll_loading: bool,
    pub editor: Option<Editor>,
    pub detail: Option<Title>,
    detail_origin: Page,
    pub episodes: Vec<catalog::Episode>,
    detail_generation: u64,
    pub detail_loading: bool,
    pub detail_focus: usize,
    pub sources: Vec<SourceChoice>,
    pub source_focus: usize,
    pub sources_loading: usize,
    sources_active: usize,
    source_queue: Vec<(Addon, String)>,
    pub video_id: String,
    pub search_term: String,
    pub search_titles: Vec<Title>,
    pub search_focus: usize,
    pub search_loading: usize,
    search_active: usize,
    search_queue: Vec<(Addon, catalog::Catalog, String)>,
    search_generation: u64,
    pub busy: bool,
    pub banner: String,
    playback: Option<PlaybackSelection>,
}

impl App {
    pub fn new(cloud: Arc<CloudClient>, transport: Arc<dyn Transport>) -> Self {
        let (workers, events) = Workers::new(4, 16);
        let mut app = Self {
            events,
            workers,
            cloud,
            transport,
            epoch: Arc::new(AtomicU64::new(1)),
            page: Page::Login,
            account: None,
            active_profile: None,
            home: ProfileHome::default(),
            addons: Vec::new(),
            order: HomeOrder::default(),
            manifest_queue: Vec::new(),
            addon_order: std::collections::HashMap::new(),
            manifest_loading: 0,
            profile_focus: 0,
            row_focus: 0,
            card_focus: 0,
            rail_focus: None,
            login_focus: 0,
            email: String::new(),
            password: Secret::new(""),
            login_code: None,
            next_poll: Instant::now(),
            poll_loading: false,
            editor: None,
            detail: None,
            detail_origin: Page::Home,
            episodes: Vec::new(),
            detail_generation: 0,
            detail_loading: false,
            detail_focus: 0,
            sources: Vec::new(),
            source_focus: 0,
            sources_loading: 0,
            sources_active: 0,
            source_queue: Vec::new(),
            video_id: String::new(),
            search_term: String::new(),
            search_titles: Vec::new(),
            search_focus: 0,
            search_loading: 0,
            search_active: 0,
            search_queue: Vec::new(),
            search_generation: 0,
            busy: false,
            banner: String::new(),
            playback: None,
        };
        if app.cloud.vault.snapshot().is_some() {
            app.fetch_profiles();
        }
        app
    }
    fn current_epoch(&self) -> u64 {
        self.epoch.load(Ordering::Acquire)
    }
    fn fail(&mut self, error: AppError) {
        if error == AppError::SessionExpired {
            self.logout();
        }
        self.banner = error.to_string();
        self.busy = false;
    }

    pub fn submit_password(&mut self, email: &str, password: Secret) {
        if self.busy {
            return;
        }
        let cloud = self.cloud.clone();
        let email = email.to_owned();
        self.epoch.fetch_add(1, Ordering::AcqRel);
        self.poll_loading = false;
        let epoch = self.current_epoch();
        let generation = cloud.vault.generation();
        self.busy = true;
        self.banner.clear();
        self.login_code = None;
        if let Err(error) = self.workers.submit(move |output| {
            let _ = output.send(Reply::Login(
                epoch,
                generation,
                cloud.password_login(&email, &password),
            ));
        }) {
            self.fail(error);
        }
    }
    fn start_device_login(&mut self) {
        if self.busy {
            return;
        }
        self.busy = true;
        self.banner.clear();
        self.epoch.fetch_add(1, Ordering::AcqRel);
        self.login_code = None;
        self.poll_loading = false;
        let cloud = self.cloud.clone();
        let epoch = self.current_epoch();
        if let Err(error) = self.workers.submit(move |output| {
            let _ = output.send(Reply::Device(epoch, cloud.start_device_login()));
        }) {
            self.fail(error);
        }
    }
    fn fetch_profiles(&mut self) {
        self.busy = true;
        let cloud = self.cloud.clone();
        let generation = cloud.vault.generation();
        let epoch = self.current_epoch();
        if let Err(error) = self.workers.submit(move |output| {
            let _ = output.send(Reply::Profiles(
                epoch,
                catalog::load_profiles(&cloud, generation),
            ));
        }) {
            self.fail(error);
        }
    }

    pub fn open_profiles(&mut self) {
        self.editor = None;
        if self.account.is_some() {
            self.page = Page::Profiles;
            self.profile_focus = self.active_profile.unwrap_or(0);
            self.banner.clear();
        } else if self.cloud.vault.snapshot().is_some() {
            self.fetch_profiles();
        }
    }
    pub fn select_profile(&mut self, index: usize) {
        let Some(account) = &self.account else {
            return;
        };
        let Some(profile) = account.profiles.get(index) else {
            return;
        };
        if profile.has_pin {
            self.profile_focus = index;
            self.edit(EditPurpose::Pin);
        } else {
            self.enter_profile(index);
        }
    }
    fn enter_profile(&mut self, index: usize) {
        let Some(account) = &self.account else {
            return;
        };
        let Some(profile) = account.profiles.get(index) else {
            return;
        };
        let ticket = match self.home.begin(&account.owner, profile, &account.profiles) {
            Ok(ticket) => ticket,
            Err(_) => {
                self.fail(AppError::InvalidResponse);
                return;
            }
        };
        self.epoch.fetch_add(1, Ordering::AcqRel);
        self.active_profile = Some(index);
        self.page = Page::Home;
        self.banner.clear();
        self.row_focus = 0;
        self.card_focus = 0;
        self.rail_focus = None;
        self.addons.clear();
        self.order = HomeOrder::default();
        self.manifest_queue.clear();
        self.manifest_loading = 0;
        self.detail = None;
        self.episodes.clear();
        self.source_queue.clear();
        self.search_queue.clear();
        self.sources_loading = 0;
        self.sources_active = 0;
        self.search_loading = 0;
        self.search_active = 0;
        self.sources.clear();
        self.search_titles.clear();
        self.playback = None;
        self.busy = true;
        let cloud = self.cloud.clone();
        let generation = cloud.vault.generation();
        let epoch = self.current_epoch();
        if let Err(error) = self.workers.submit(move |output| {
            let result = (|| {
                let addons = catalog::installed_addons(&cloud, generation, &ticket)?;
                let order = catalog::home_order(&cloud, generation, &ticket)?;
                Ok((addons, order))
            })();
            let _ = output.send(Reply::Seed(epoch, ticket, result));
        }) {
            self.fail(error);
        }
    }

    fn rebuild_rows(&mut self) {
        let focused = self
            .home
            .rows
            .get(self.row_focus)
            .map(|row| row.key.clone());
        let mut old: std::collections::HashMap<_, _> = std::mem::take(&mut self.home.rows)
            .into_iter()
            .map(|row| (row.key.clone(), row))
            .collect();
        self.home.rows = self
            .order
            .rows(&self.addons)
            .into_iter()
            .map(|row| old.remove(&row.key).unwrap_or(row))
            .collect();
        if let Some(key) = focused {
            if let Some(index) = self.home.rows.iter().position(|row| row.key == key) {
                self.row_focus = index;
            }
        }
        self.row_focus = self.row_focus.min(self.home.rows.len().saturating_sub(1));
    }

    fn pump_manifests(&mut self) {
        while self.manifest_loading < 4 && !self.manifest_queue.is_empty() {
            let Some(ticket) = self.home.ticket().cloned() else {
                break;
            };
            let record = self
                .manifest_queue
                .last()
                .expect("pending manifest")
                .clone();
            let service = CatalogService::new(self.transport.clone());
            let epoch = self.current_epoch();
            let cancelled = self.epoch.clone();
            if self
                .workers
                .submit(move |output| {
                    if cancelled.load(Ordering::Acquire) != epoch {
                        return;
                    }
                    let _ = output.send(Reply::Manifest(epoch, ticket, service.manifest(record)));
                })
                .is_err()
            {
                break;
            }
            self.manifest_queue.pop();
            self.manifest_loading += 1;
        }
    }
    fn pump_rows(&mut self) {
        if self.page != Page::Home {
            return;
        }
        let end = (self.row_focus + 3).min(self.home.rows.len());
        for index in self.row_focus.saturating_sub(1)..end {
            let row = &self.home.rows[index];
            if row.loaded || row.loading || row.error.is_some() {
                continue;
            }
            let row = row.clone();
            let Some(ticket) = self.home.ticket().cloned() else {
                return;
            };
            let epoch = self.current_epoch();
            let cancelled = self.epoch.clone();
            let service = CatalogService::new(self.transport.clone());
            if self
                .workers
                .submit(move |output| {
                    if cancelled.load(Ordering::Acquire) != epoch {
                        return;
                    }
                    let result = service.row(&row, 0);
                    let _ = output.send(Reply::Row(epoch, ticket, row.key, 0, result));
                })
                .is_ok()
            {
                self.home.rows[index].loading = true;
            }
        }
    }
    fn next_page(&mut self) {
        let Some(row) = self.home.rows.get(self.row_focus) else {
            return;
        };
        if row.loading || row.exhausted || !row.catalog.supports_extra("skip") {
            return;
        }
        let row = row.clone();
        let skip = row.titles.len();
        let Some(ticket) = self.home.ticket().cloned() else {
            return;
        };
        let epoch = self.current_epoch();
        let service = CatalogService::new(self.transport.clone());
        if self
            .workers
            .submit(move |output| {
                let result = service.row(&row, skip);
                let _ = output.send(Reply::Row(epoch, ticket, row.key, skip, result));
            })
            .is_ok()
        {
            self.home.rows[self.row_focus].loading = true;
        }
    }

    pub fn focused_title(&self) -> Option<&Title> {
        match self.page {
            Page::Home => self
                .home
                .rows
                .get(self.row_focus)
                .and_then(|row| row.titles.get(self.card_focus)),
            Page::Search => self.search_titles.get(self.search_focus),
            _ => self.detail.as_ref(),
        }
    }
    pub fn open_title(&mut self, title: Title) {
        self.detail_origin = if self.page == Page::Search {
            Page::Search
        } else {
            Page::Home
        };
        self.episodes = title.sorted_episodes();
        self.detail_generation = self.detail_generation.wrapping_add(1);
        self.detail = Some(title.clone());
        self.detail_focus = 0;
        self.detail_loading = true;
        self.page = Page::Detail;
        self.banner.clear();
        self.sources.clear();
        let mut addons: Vec<_> = self
            .addons
            .iter()
            .filter(|addon| addon.manifest.supports("meta", &title.kind, &title.id))
            .cloned()
            .collect();
        addons.extend(metadata_fallbacks());
        let service = CatalogService::new(self.transport.clone());
        let epoch = self.current_epoch();
        let generation = self.detail_generation;
        let cancelled = self.epoch.clone();
        if let Err(error) = self.workers.submit(move |output| {
            let mut result = Err(AppError::InvalidResponse);
            for addon in addons {
                if cancelled.load(Ordering::Acquire) != epoch {
                    return;
                }
                result = service.detail(&addon, &title.kind, &title.id);
                if result.is_ok() {
                    break;
                }
            }
            let _ = output.send(Reply::Detail(epoch, generation, result));
        }) {
            self.detail_loading = false;
            self.fail(error);
        }
    }
    pub fn load_sources(&mut self, video_id: String) {
        let Some(title) = &self.detail else {
            return;
        };
        self.source_queue = self
            .addons
            .iter()
            .filter(|addon| addon.manifest.supports("stream", &title.kind, &video_id))
            .cloned()
            .map(|addon| (addon, video_id.clone()))
            .collect();
        self.source_queue.reverse();
        self.sources_loading = self.source_queue.len();
        self.sources_active = 0;
        self.page = Page::Sources;
        self.video_id = video_id;
        self.sources.clear();
        self.source_focus = 0;
        self.banner.clear();
        self.playback = None;
        self.detail_generation = self.detail_generation.wrapping_add(1);
    }
    fn pump_sources(&mut self) {
        if self.page != Page::Sources {
            return;
        }
        while self.sources_active < 4 {
            let Some((addon, id)) = self.source_queue.last().cloned() else {
                break;
            };
            let Some(title) = &self.detail else {
                break;
            };
            let kind = title.kind.clone();
            let service = CatalogService::new(self.transport.clone());
            let epoch = self.current_epoch();
            let generation = self.detail_generation;
            if self
                .workers
                .submit(move |output| {
                    let _ = output.send(Reply::Sources(
                        epoch,
                        generation,
                        service.sources(&addon, &kind, &id),
                    ));
                })
                .is_err()
            {
                break;
            }
            self.source_queue.pop();
            self.sources_active += 1;
        }
    }
    pub fn take_playback(&mut self) -> Option<PlaybackSelection> {
        self.playback.take()
    }

    pub fn search(&mut self, term: &str) {
        self.page = Page::Search;
        self.search_term = term.to_owned();
        self.search_titles.clear();
        self.search_focus = 0;
        self.banner.clear();
        self.search_generation = self.search_generation.wrapping_add(1);
        self.search_queue.clear();
        self.search_active = 0;
        self.search_loading = 0;
        if term.trim().is_empty() {
            return;
        }
        let mut addons = self.addons.clone();
        addons.extend(metadata_fallbacks());
        for addon in addons {
            for catalog in addon
                .manifest
                .catalogs
                .iter()
                .filter(|catalog| catalog.supports_extra("search"))
            {
                self.search_queue
                    .push((addon.clone(), catalog.clone(), term.to_owned()));
            }
        }
        self.search_queue.reverse();
        self.search_loading = self.search_queue.len();
    }
    fn pump_search(&mut self) {
        if self.page != Page::Search {
            return;
        }
        while self.search_active < 4 {
            let Some((addon, catalog, term)) = self.search_queue.last().cloned() else {
                break;
            };
            let service = CatalogService::new(self.transport.clone());
            let epoch = self.current_epoch();
            let generation = self.search_generation;
            if self
                .workers
                .submit(move |output| {
                    let _ = output.send(Reply::Search(
                        epoch,
                        generation,
                        service.search(&addon, &catalog, &term),
                    ));
                })
                .is_err()
            {
                break;
            }
            self.search_queue.pop();
            self.search_active += 1;
        }
    }
    pub fn tick(&mut self) {
        while let Ok(reply) = self.events.try_recv() {
            let epoch = match &reply {
                Reply::Login(e, _, _)
                | Reply::Device(e, _)
                | Reply::Poll(e, _, _)
                | Reply::Profiles(e, _)
                | Reply::Seed(e, _, _)
                | Reply::Manifest(e, _, _)
                | Reply::Row(e, _, _, _, _)
                | Reply::Detail(e, _, _)
                | Reply::Sources(e, _, _)
                | Reply::Search(e, _, _)
                | Reply::Pin(e, _, _) => *e,
            };
            if epoch != self.current_epoch() {
                continue;
            }
            match reply {
                Reply::Login(_, generation, result) => self.accept_login(generation, result),
                Reply::Device(_, result) => {
                    self.busy = false;
                    match result {
                        Ok(code) => {
                            self.login_code = Some(Arc::new(code));
                            self.next_poll = Instant::now();
                        }
                        Err(error) => self.fail(error),
                    }
                }
                Reply::Poll(_, generation, result) => {
                    self.poll_loading = false;
                    match result {
                        Ok(Some(session)) => self.accept_login(generation, Ok(session)),
                        Ok(None) => {}
                        Err(error) => {
                            self.banner = error.to_string();
                            if error == AppError::LoginExpired {
                                self.login_code = None;
                            }
                        }
                    }
                }
                Reply::Profiles(_, result) => {
                    self.busy = false;
                    match result {
                        Ok(account) => {
                            self.account = Some(account);
                            self.page = Page::Profiles;
                            self.profile_focus = 0;
                        }
                        Err(error) => self.fail(error),
                    }
                }
                Reply::Seed(_, ticket, result) => {
                    if !self.home.accepts(&ticket) {
                        continue;
                    }
                    match result {
                        Ok((mut records, order)) => {
                            self.addon_order = records
                                .iter()
                                .enumerate()
                                .map(|(index, record)| (record.url.clone(), index))
                                .collect();
                            records.reverse();
                            self.manifest_queue = records;
                            self.order = order;
                            self.busy = false;
                        }
                        Err(error) => self.fail(error),
                    }
                }
                Reply::Manifest(_, ticket, result) => {
                    if !self.home.accepts(&ticket) {
                        continue;
                    }
                    self.manifest_loading = self.manifest_loading.saturating_sub(1);
                    match result {
                        Ok(addon) => {
                            self.addons.push(addon);
                            self.addons.sort_by_key(|addon| {
                                self.addon_order
                                    .get(&addon.record.url)
                                    .copied()
                                    .unwrap_or(usize::MAX)
                            });
                            self.rebuild_rows();
                        }
                        Err(error) => {
                            self.banner = format!("An addon could not be loaded. {error}")
                        }
                    }
                }
                Reply::Row(_, ticket, key, skip, result) => {
                    if !self.home.accepts(&ticket) {
                        continue;
                    }
                    if let Some(row) = self.home.rows.iter_mut().find(|row| row.key == key) {
                        row.loading = false;
                        row.loaded = true;
                        match result {
                            Ok(titles) => {
                                let previous_count = row.titles.len();
                                if skip == 0 {
                                    row.titles = titles;
                                } else {
                                    let mut ids: HashSet<_> =
                                        row.titles.iter().map(|title| title.id.clone()).collect();
                                    row.titles.extend(
                                        titles
                                            .into_iter()
                                            .filter(|title| ids.insert(title.id.clone())),
                                    );
                                }
                                row.exhausted = row.titles.is_empty()
                                    || (skip > 0 && row.titles.len() == previous_count);
                            }
                            Err(error) => row.error = Some(error),
                        }
                    }
                }
                Reply::Detail(_, generation, result) => {
                    if generation != self.detail_generation {
                        continue;
                    }
                    self.detail_loading = false;
                    match result {
                        Ok(title) => {
                            self.episodes = title.sorted_episodes();
                            self.detail = Some(title);
                        }
                        Err(error) => self.banner = error.to_string(),
                    }
                }
                Reply::Sources(_, generation, result) => {
                    if generation != self.detail_generation || self.page != Page::Sources {
                        continue;
                    }
                    self.sources_loading = self.sources_loading.saturating_sub(1);
                    self.sources_active = self.sources_active.saturating_sub(1);
                    match result {
                        Ok(sources) => self.sources.extend(sources),
                        Err(error) => {
                            self.banner = format!("An addon did not return sources. {error}")
                        }
                    }
                    // Receiving sources never initiates playback. Only Accept on a row can do that.
                }
                Reply::Search(_, generation, result) => {
                    if generation != self.search_generation {
                        continue;
                    }
                    self.search_loading = self.search_loading.saturating_sub(1);
                    self.search_active = self.search_active.saturating_sub(1);
                    match result {
                        Ok(titles) => {
                            let mut ids: HashSet<_> = self
                                .search_titles
                                .iter()
                                .map(|title| (title.kind.clone(), title.id.clone()))
                                .collect();
                            self.search_titles
                                .extend(titles.into_iter().filter(|title| {
                                    ids.insert((title.kind.clone(), title.id.clone()))
                                }));
                        }
                        Err(error) => {
                            self.banner = format!("One search provider did not respond. {error}")
                        }
                    }
                }
                Reply::Pin(_, index, result)
                    if self.page == Page::Profiles && self.profile_focus == index =>
                {
                    match result {
                        Ok(()) => {
                            self.busy = false;
                            self.enter_profile(index);
                        }
                        Err(error) => self.fail(error),
                    }
                }
                Reply::Pin(..) => {}
            }
        }
        self.pump_manifests();
        self.pump_rows();
        self.pump_sources();
        self.pump_search();
        if self.page == Page::Login && !self.poll_loading && Instant::now() >= self.next_poll {
            let Some(login) = self.login_code.clone() else {
                return;
            };
            let cloud = self.cloud.clone();
            let epoch = self.current_epoch();
            let generation = cloud.vault.generation();
            self.next_poll = Instant::now() + Duration::from_secs(login.poll_interval_seconds);
            if self
                .workers
                .submit(move |output| {
                    let _ = output.send(Reply::Poll(
                        epoch,
                        generation,
                        cloud.poll_device_login(&login),
                    ));
                })
                .is_ok()
            {
                self.poll_loading = true;
            }
        }
    }
    fn accept_login(&mut self, generation: u64, result: Result<Session>) {
        self.busy = false;
        match result.and_then(|session| self.cloud.vault.install(generation, session)) {
            Ok(_) => {
                self.password = Secret::new("");
                self.login_code = None;
                self.fetch_profiles();
            }
            Err(error) => self.fail(error),
        }
    }

    pub fn logout(&mut self) {
        self.epoch.fetch_add(1, Ordering::AcqRel);
        self.home.clear();
        self.account = None;
        self.active_profile = None;
        self.addons.clear();
        self.manifest_queue.clear();
        self.detail = None;
        self.episodes.clear();
        self.source_queue.clear();
        self.search_queue.clear();
        self.sources_loading = 0;
        self.sources_active = 0;
        self.search_loading = 0;
        self.search_active = 0;
        self.sources.clear();
        self.search_titles.clear();
        self.login_code = None;
        self.editor = None;
        self.password = Secret::new("");
        self.page = Page::Login;
        self.busy = false;
        self.poll_loading = false;
        self.playback = None;
        self.banner.clear();
        if let Err(error) = self.cloud.vault.logout() {
            self.fail(error);
        }
    }
    fn edit(&mut self, purpose: EditPurpose) {
        let value = match purpose {
            EditPurpose::Email => self.email.clone(),
            EditPurpose::Password => self.password.expose().to_owned(),
            EditPurpose::Search => self.search_term.clone(),
            EditPurpose::Pin => String::new(),
        };
        self.editor = Some(Editor {
            purpose,
            value: Zeroizing::new(value),
            focus: 0,
            shift: false,
        });
    }
    pub fn text_input(&mut self, text: &str) {
        if let Some(editor) = &mut self.editor {
            for character in text.chars().filter(|character| !character.is_control()) {
                if editor.value.len() + character.len_utf8() <= 2048
                    && (editor.purpose != EditPurpose::Pin || character.is_ascii_digit())
                {
                    editor.value.push(character);
                }
            }
        }
    }
    fn finish_edit(&mut self) {
        let Some(editor) = self.editor.take() else {
            return;
        };
        match editor.purpose {
            EditPurpose::Email => self.email = editor.value.to_string(),
            EditPurpose::Password => self.password = Secret::new(editor.value.to_string()),
            EditPurpose::Search => self.search(&editor.value),
            EditPurpose::Pin => {
                let index = self.profile_focus;
                let Some(account) = &self.account else {
                    return;
                };
                let id = account.profiles[index].profile_index;
                let cloud = self.cloud.clone();
                let generation = cloud.vault.generation();
                let epoch = self.current_epoch();
                self.busy = true;
                if let Err(error) = self.workers.submit(move |output| {
                    let _ = output.send(Reply::Pin(
                        epoch,
                        index,
                        catalog::verify_profile_pin(&cloud, generation, id, &editor.value),
                    ));
                }) {
                    self.fail(error);
                }
            }
        }
    }

    pub fn input(&mut self, input: Input) {
        if self.editor.is_some() {
            let editor = self.editor.as_mut().expect("text editor");
            match input {
                Input::Back => self.editor = None,
                Input::Delete => {
                    editor.value.pop();
                }
                Input::Confirm => self.finish_edit(),
                Input::Up => editor.focus = editor.focus.saturating_sub(12),
                Input::Down => editor.focus = (editor.focus + 12).min(KEYS.len() - 1),
                Input::Left => editor.focus = editor.focus.saturating_sub(1),
                Input::Right => editor.focus = (editor.focus + 1).min(KEYS.len() - 1),
                Input::Accept => match KEYS[editor.focus] {
                    "Done" => self.finish_edit(),
                    "Delete" => {
                        editor.value.pop();
                    }
                    "Shift" => editor.shift = !editor.shift,
                    key => {
                        let key = if editor.shift {
                            key.to_uppercase()
                        } else {
                            key.to_owned()
                        };
                        self.text_input(&key);
                    }
                },
                _ => {}
            }
            return;
        }
        if input == Input::Profiles {
            self.open_profiles();
            return;
        }
        if input == Input::Search && self.active_profile.is_some() {
            self.edit(EditPurpose::Search);
            return;
        }
        match self.page {
            Page::Login => match input {
                Input::Up => self.login_focus = self.login_focus.saturating_sub(1),
                Input::Down => self.login_focus = (self.login_focus + 1).min(3),
                Input::Accept => match self.login_focus {
                    0 => self.edit(EditPurpose::Email),
                    1 => self.edit(EditPurpose::Password),
                    2 => {
                        let email = self.email.clone();
                        self.submit_password(&email, self.password.clone());
                    }
                    _ => self.start_device_login(),
                },
                Input::Back => {
                    self.login_code = None;
                    self.epoch.fetch_add(1, Ordering::AcqRel);
                    self.busy = false;
                    self.poll_loading = false;
                }
                _ => {}
            },
            Page::Profiles => match input {
                Input::Left | Input::Up => {
                    self.profile_focus = self.profile_focus.saturating_sub(1)
                }
                Input::Right | Input::Down => {
                    let count = self
                        .account
                        .as_ref()
                        .map(|account| account.profiles.len())
                        .unwrap_or(0);
                    self.profile_focus = (self.profile_focus + 1).min(count.saturating_sub(1));
                }
                Input::Accept => self.select_profile(self.profile_focus),
                Input::Back if self.active_profile.is_some() => {
                    self.page = Page::Home;
                    self.busy = false;
                }
                _ => {}
            },
            Page::Home => self.home_input(input),
            Page::Detail => match input {
                Input::Up => self.detail_focus = self.detail_focus.saturating_sub(1),
                Input::Down => {
                    let count = self.episodes.len();
                    self.detail_focus = (self.detail_focus + 1).min(count.saturating_sub(1));
                }
                Input::Accept if !self.detail_loading => {
                    if let Some(title) = &self.detail {
                        let id = if title.kind == "series" {
                            self.episodes
                                .get(self.detail_focus)
                                .map(|episode| episode.id.clone())
                        } else {
                            Some(title.id.clone())
                        };
                        if let Some(id) = id {
                            self.load_sources(id);
                        }
                    }
                }
                Input::Back => {
                    self.page = self.detail_origin;
                    self.detail_generation = self.detail_generation.wrapping_add(1);
                }
                _ => {}
            },
            Page::Sources => match input {
                Input::Up => self.source_focus = self.source_focus.saturating_sub(1),
                Input::Down => {
                    self.source_focus =
                        (self.source_focus + 1).min(self.sources.len().saturating_sub(1))
                }
                Input::Accept => {
                    if let (Some(title), Some(choice)) = (
                        self.detail.clone(),
                        self.sources.get(self.source_focus).cloned(),
                    ) {
                        self.playback = Some(PlaybackSelection {
                            title,
                            video_id: self.video_id.clone(),
                            choice,
                        });
                    }
                }
                Input::Back => {
                    self.page = Page::Detail;
                    self.source_queue.clear();
                    self.sources_loading = 0;
                    self.sources_active = 0;
                    self.detail_generation = self.detail_generation.wrapping_add(1);
                }
                _ => {}
            },
            Page::Search => match input {
                Input::Left => self.search_focus = self.search_focus.saturating_sub(1),
                Input::Right => {
                    self.search_focus =
                        (self.search_focus + 1).min(self.search_titles.len().saturating_sub(1))
                }
                Input::Up => self.search_focus = self.search_focus.saturating_sub(7),
                Input::Down => {
                    self.search_focus =
                        (self.search_focus + 7).min(self.search_titles.len().saturating_sub(1))
                }
                Input::Accept => {
                    if let Some(title) = self.search_titles.get(self.search_focus).cloned() {
                        self.open_title(title);
                    }
                }
                Input::Back => self.page = Page::Home,
                _ => {}
            },
        }
    }
    fn home_input(&mut self, input: Input) {
        if let Some(focus) = self.rail_focus {
            match input {
                Input::Up => self.rail_focus = Some(focus.saturating_sub(1)),
                Input::Down => self.rail_focus = Some((focus + 1).min(2)),
                Input::Right | Input::Back => self.rail_focus = None,
                Input::Accept => match focus {
                    0 => self.rail_focus = None,
                    1 => self.edit(EditPurpose::Search),
                    _ => self.open_profiles(),
                },
                _ => {}
            }
            return;
        }
        match input {
            Input::Left => {
                if self.card_focus == 0 {
                    self.rail_focus = Some(0);
                } else {
                    self.card_focus -= 1;
                }
            }
            Input::Right => {
                let count = self
                    .home
                    .rows
                    .get(self.row_focus)
                    .map(|row| row.titles.len())
                    .unwrap_or(0);
                self.card_focus = (self.card_focus + 1).min(count.saturating_sub(1));
                if self.card_focus + 5 >= count {
                    self.next_page();
                }
            }
            Input::Up => {
                self.row_focus = self.row_focus.saturating_sub(1);
                self.card_focus = 0;
            }
            Input::Down => {
                self.row_focus = (self.row_focus + 1).min(self.home.rows.len().saturating_sub(1));
                self.card_focus = 0;
            }
            Input::Accept => {
                if let Some(title) = self.focused_title().cloned() {
                    self.open_title(title);
                }
            }
            Input::Back => self.rail_focus = Some(0),
            _ => {}
        }
    }
    pub fn catalogs_loading(&self) -> bool {
        self.busy || self.manifest_loading > 0 || !self.manifest_queue.is_empty()
    }
}

impl Drop for App {
    fn drop(&mut self) {
        self.epoch.fetch_add(1, Ordering::AcqRel);
    }
}

fn metadata_fallbacks() -> Vec<Addon> {
    [
        ("nuvio.metadata","https://catalog.nuvio.tv/%7B%22language%22%3A%22en-US%22%7D/manifest.json", "popular-movies", "popular-series"),
        ("cinemeta.metadata","https://v3-cinemeta.strem.io/manifest.json", "top", "top"),
    ].into_iter().map(|(id,url,movie,series)| Addon {
        record:Arc::new(InstalledAddon {url:url.to_owned(),name:id.to_owned(),enabled:true}),
        manifest:Arc::new(serde_json::from_value(json!({"id":id,"name":id,"types":["movie","series"],"resources":["meta"],"catalogs":[
            {"id":movie,"type":"movie","extra":[{"name":"search","isRequired":true}]},{"id":series,"type":"series","extra":[{"name":"search","isRequired":true}]}]})).expect("built-in metadata contract")),
    }).collect()
}
