use crate::{
    account::CloudClient,
    error::{AppError, Result},
    http::{parse_service_url, HttpRequest, Transport},
    profile::{HomeTicket, Profile},
};
use serde::{Deserialize, Deserializer, Serialize};
use serde_json::{json, Value};
use std::{
    collections::{HashMap, HashSet},
    sync::Arc,
};
use url::Url;

fn null_default<'de, D: Deserializer<'de>, T: Deserialize<'de> + Default>(
    deserializer: D,
) -> std::result::Result<T, D::Error> {
    Ok(Option::<T>::deserialize(deserializer)?.unwrap_or_default())
}

fn enabled() -> bool {
    true
}

#[derive(Clone, Deserialize, Serialize)]
pub struct InstalledAddon {
    pub url: String,
    #[serde(default, deserialize_with = "null_default")]
    pub name: String,
    #[serde(default = "enabled")]
    pub enabled: bool,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct Manifest {
    pub id: String,
    #[serde(default, deserialize_with = "null_default")]
    pub name: String,
    #[serde(default, deserialize_with = "null_default")]
    pub types: Vec<String>,
    #[serde(default, deserialize_with = "null_default")]
    pub catalogs: Vec<Catalog>,
    #[serde(default, deserialize_with = "null_default")]
    pub resources: Vec<Resource>,
    #[serde(default, rename = "idPrefixes", deserialize_with = "null_default")]
    pub id_prefixes: Vec<String>,
}

#[derive(Clone, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Resource {
    Name(String),
    Detailed {
        name: String,
        types: Option<Vec<String>>,
        #[serde(rename = "idPrefixes")]
        id_prefixes: Option<Vec<String>>,
    },
}

impl Manifest {
    pub fn supports(&self, resource: &str, kind: &str, id: &str) -> bool {
        self.resources.iter().any(|r| {
            let (name, types, prefixes) = match r {
                Resource::Name(name) => (name, &self.types, &self.id_prefixes),
                Resource::Detailed {
                    name,
                    types,
                    id_prefixes,
                } => (
                    name,
                    types.as_ref().unwrap_or(&self.types),
                    id_prefixes.as_ref().unwrap_or(&self.id_prefixes),
                ),
            };
            name == resource
                && (types.is_empty() || types.iter().any(|t| t == kind))
                && (prefixes.is_empty() || prefixes.iter().any(|prefix| id.starts_with(prefix)))
        })
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub struct Catalog {
    pub id: String,
    #[serde(rename = "type")]
    pub kind: String,
    #[serde(default, deserialize_with = "null_default")]
    pub name: String,
    #[serde(default, deserialize_with = "null_default")]
    pub extra: Vec<CatalogExtra>,
    #[serde(default, rename = "extraSupported", deserialize_with = "null_default")]
    pub extra_supported: Vec<String>,
    #[serde(default, rename = "extraRequired", deserialize_with = "null_default")]
    pub extra_required: Vec<String>,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct CatalogExtra {
    pub name: String,
    #[serde(default, rename = "isRequired")]
    pub is_required: bool,
    #[serde(default, deserialize_with = "null_default")]
    pub options: Vec<String>,
}

impl Catalog {
    pub fn supports_extra(&self, name: &str) -> bool {
        self.extra_supported.iter().any(|extra| extra == name)
            || self.extra.iter().any(|extra| extra.name == name)
    }
    pub fn requires_extra(&self, name: &str) -> bool {
        self.extra_required.iter().any(|extra| extra == name)
            || self
                .extra
                .iter()
                .any(|extra| extra.name == name && extra.is_required)
    }
    pub fn home_extras(&self) -> Option<Vec<(String, String)>> {
        if self.requires_extra("search") {
            return None;
        }
        let mut result = Vec::new();
        for extra in &self.extra {
            if extra.is_required && extra.name != "skip" {
                let value = extra.options.first()?.clone();
                result.push((extra.name.clone(), value));
            }
        }
        if self
            .extra_required
            .iter()
            .any(|name| name != "skip" && !result.iter().any(|(key, _)| key == name))
        {
            return None;
        }
        Some(result)
    }
}

#[derive(Clone)]
pub struct Addon {
    pub record: Arc<InstalledAddon>,
    pub manifest: Arc<Manifest>,
}

#[derive(Clone)]
pub struct HomeRow {
    pub key: String,
    pub name: String,
    pub addon: Addon,
    pub catalog: Catalog,
    pub extras: Vec<(String, String)>,
    pub titles: Vec<Title>,
    pub loaded: bool,
    pub loading: bool,
    pub error: Option<AppError>,
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct Title {
    #[serde(default, deserialize_with = "null_default")]
    pub id: String,
    #[serde(default, rename = "type", deserialize_with = "null_default")]
    pub kind: String,
    #[serde(default, deserialize_with = "null_default")]
    pub name: String,
    pub poster: Option<String>,
    pub background: Option<String>,
    pub logo: Option<String>,
    #[serde(default, deserialize_with = "null_default")]
    pub description: String,
    #[serde(default, rename = "releaseInfo", deserialize_with = "null_default")]
    pub release_info: String,
    #[serde(default, rename = "imdbRating", deserialize_with = "rating")]
    pub imdb_rating: String,
    #[serde(default, deserialize_with = "null_default")]
    pub genres: Vec<String>,
    #[serde(default, deserialize_with = "null_default")]
    pub runtime: String,
    #[serde(default, deserialize_with = "null_default")]
    pub videos: Vec<Episode>,
    #[serde(default, deserialize_with = "null_default")]
    pub cast: Vec<String>,
    #[serde(default, deserialize_with = "null_default")]
    pub director: Vec<String>,
}

fn rating<'de, D: Deserializer<'de>>(deserializer: D) -> std::result::Result<String, D::Error> {
    let value = Value::deserialize(deserializer)?;
    Ok(match value {
        Value::String(text) => text,
        Value::Number(number) => number.to_string(),
        _ => String::new(),
    })
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct Episode {
    #[serde(default, deserialize_with = "null_default")]
    pub id: String,
    #[serde(
        default,
        rename = "title",
        alias = "name",
        deserialize_with = "null_default"
    )]
    pub name: String,
    pub season: Option<u32>,
    pub episode: Option<u32>,
    pub released: Option<String>,
    pub thumbnail: Option<String>,
    #[serde(default, deserialize_with = "null_default")]
    pub overview: String,
    #[serde(default = "enabled")]
    pub available: bool,
}

impl Title {
    pub fn valid(&self) -> bool {
        !self.id.is_empty() && !self.name.is_empty()
    }
    pub fn sorted_episodes(&self) -> Vec<Episode> {
        let mut episodes: Vec<_> = self
            .videos
            .iter()
            .filter(|e| !e.id.is_empty())
            .cloned()
            .collect();
        episodes.sort_by_key(|e| (e.season.unwrap_or(0), e.episode.unwrap_or(0)));
        episodes
    }
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct Source {
    #[serde(default, deserialize_with = "null_default")]
    pub name: String,
    #[serde(default, deserialize_with = "null_default")]
    pub description: String,
    #[serde(default, rename = "title", deserialize_with = "null_default")]
    pub title: String,
    pub url: Option<String>,
    #[serde(rename = "externalUrl")]
    pub external_url: Option<String>,
    #[serde(rename = "infoHash")]
    pub info_hash: Option<String>,
    #[serde(default, rename = "behaviorHints", deserialize_with = "null_default")]
    pub behavior: SourceBehavior,
    #[serde(default, deserialize_with = "null_default")]
    pub subtitles: Vec<Subtitle>,
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct SourceBehavior {
    #[serde(default, rename = "proxyHeaders", deserialize_with = "null_default")]
    pub proxy_headers: ProxyHeaders,
    #[serde(rename = "filename")]
    pub filename: Option<String>,
    #[serde(rename = "videoSize")]
    pub video_size: Option<u64>,
}

#[derive(Clone, Default, Deserialize, Serialize)]
pub struct ProxyHeaders {
    #[serde(default, deserialize_with = "null_default")]
    pub request: HashMap<String, String>,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct Subtitle {
    pub id: String,
    pub url: String,
    pub lang: String,
}

#[derive(Clone)]
pub struct SourceChoice {
    pub addon_name: String,
    pub source: Source,
}

#[derive(Deserialize)]
struct CatalogResponse {
    #[serde(default, deserialize_with = "null_default")]
    metas: Vec<Title>,
}
#[derive(Deserialize)]
struct MetaResponse {
    meta: Title,
}
#[derive(Deserialize)]
struct StreamResponse {
    #[serde(default, deserialize_with = "null_default")]
    streams: Vec<Source>,
}

/// Build endpoint paths as URL segments, retaining query-based addon credentials.
pub fn addon_endpoint(
    manifest_url: &str,
    resource: Option<(&str, &str, &str)>,
    extras: &[(String, String)],
) -> Result<Url> {
    let mut url = parse_service_url(manifest_url)?;
    let mut segments = url.path_segments_mut().map_err(|_| AppError::InvalidUrl)?;
    segments.pop_if_empty();
    // The stored URL may be a base URL or a complete manifest URL.
    drop(segments);
    if url.path().ends_with("/manifest.json") {
        url.path_segments_mut()
            .map_err(|_| AppError::InvalidUrl)?
            .pop();
    }
    let mut segments = url.path_segments_mut().map_err(|_| AppError::InvalidUrl)?;
    match resource {
        None => {
            segments.push("manifest.json");
        }
        Some((resource, kind, id)) => {
            segments.push(resource).push(kind);
            if extras.is_empty() {
                segments.push(&format!("{id}.json"));
            } else {
                segments.push(id);
                let encoded = url::form_urlencoded::Serializer::new(String::new())
                    .extend_pairs(
                        extras
                            .iter()
                            .map(|(name, value)| (name.as_str(), value.as_str())),
                    )
                    .finish();
                // push() escapes '%' again; set the already encoded extra path after releasing the borrow.
                drop(segments);
                let path = format!("{}/{encoded}.json", url.path().trim_end_matches('/'));
                url.set_path(&path);
                return Ok(url);
            }
        }
    }
    drop(segments);
    Ok(url)
}

pub struct CatalogService {
    transport: Arc<dyn Transport>,
}

impl CatalogService {
    pub fn new(transport: Arc<dyn Transport>) -> Self {
        Self { transport }
    }
    fn get<T: serde::de::DeserializeOwned>(&self, url: Url) -> Result<T> {
        let mut request = HttpRequest::get(url);
        request.header("accept-language", "en-US,en;q=0.9")?;
        self.transport.send(request)?.json()
    }
    pub fn manifest(&self, record: InstalledAddon) -> Result<Addon> {
        let manifest: Manifest = self.get(addon_endpoint(&record.url, None, &[])?)?;
        if manifest.id.is_empty() {
            return Err(AppError::InvalidResponse);
        }
        Ok(Addon {
            record: Arc::new(record),
            manifest: Arc::new(manifest),
        })
    }
    pub fn row(&self, row: &HomeRow, skip: usize) -> Result<Vec<Title>> {
        let mut extras = row.extras.clone();
        if skip > 0 && row.catalog.supports_extra("skip") {
            extras.push(("skip".to_owned(), skip.to_string()));
        }
        let response: CatalogResponse = self.get(addon_endpoint(
            &row.addon.record.url,
            Some(("catalog", &row.catalog.kind, &row.catalog.id)),
            &extras,
        )?)?;
        let mut titles = response.metas;
        titles.retain(Title::valid);
        for title in &mut titles {
            if title.kind.is_empty() {
                title.kind.clone_from(&row.catalog.kind);
            }
        }
        Ok(titles)
    }
    pub fn detail(&self, addon: &Addon, kind: &str, id: &str) -> Result<Title> {
        let response: MetaResponse = self.get(addon_endpoint(
            &addon.record.url,
            Some(("meta", kind, id)),
            &[],
        )?)?;
        if !response.meta.valid() || response.meta.id != id {
            return Err(AppError::InvalidResponse);
        }
        let mut title = response.meta;
        if title.kind.is_empty() {
            title.kind = kind.to_owned();
        }
        Ok(title)
    }
    pub fn sources(&self, addon: &Addon, kind: &str, id: &str) -> Result<Vec<SourceChoice>> {
        let response: StreamResponse = self.get(addon_endpoint(
            &addon.record.url,
            Some(("stream", kind, id)),
            &[],
        )?)?;
        Ok(response
            .streams
            .into_iter()
            .map(|source| SourceChoice {
                addon_name: addon.manifest.name.clone(),
                source,
            })
            .collect())
    }
    pub fn search(&self, addon: &Addon, catalog: &Catalog, term: &str) -> Result<Vec<Title>> {
        let extras = vec![("search".to_owned(), term.to_owned())];
        let response: CatalogResponse = self.get(addon_endpoint(
            &addon.record.url,
            Some(("catalog", &catalog.kind, &catalog.id)),
            &extras,
        )?)?;
        let mut titles: Vec<_> = response.metas.into_iter().filter(Title::valid).collect();
        for title in &mut titles {
            if title.kind.is_empty() {
                title.kind.clone_from(&catalog.kind);
            }
        }
        Ok(titles)
    }
}

#[derive(Deserialize)]
struct ProfileLock {
    profile_id: Option<u32>,
    profile_index: Option<u32>,
    #[serde(default)]
    pin_enabled: bool,
}

pub struct AccountProfiles {
    pub owner: String,
    pub profiles: Vec<Profile>,
}

pub fn load_profiles(cloud: &CloudClient, generation: u64) -> Result<AccountProfiles> {
    let owner: String = cloud.rpc(generation, "get_sync_owner", json!({}))?;
    if owner.is_empty() || owner.len() > 128 {
        return Err(AppError::InvalidResponse);
    }
    let mut profiles: Vec<Profile> = cloud.rpc(generation, "sync_pull_profiles", json!({}))?;
    profiles.retain(|profile| profile.profile_index > 0);
    if profiles.is_empty() {
        return Err(AppError::NoProfiles);
    }
    let mut indices = HashSet::new();
    if profiles
        .iter()
        .any(|profile| !indices.insert(profile.profile_index))
    {
        return Err(AppError::InvalidResponse);
    }
    // A failed lock read must not expose a PIN-protected profile.
    let locks: Vec<ProfileLock> = cloud.rpc(generation, "sync_pull_profile_locks", json!({}))?;
    for profile in &mut profiles {
        if let Some(lock) = locks
            .iter()
            .find(|lock| lock.profile_id.or(lock.profile_index) == Some(profile.profile_index))
        {
            profile.has_pin = lock.pin_enabled;
        }
    }
    Ok(AccountProfiles { owner, profiles })
}

pub fn verify_profile_pin(
    cloud: &CloudClient,
    generation: u64,
    index: u32,
    pin: &str,
) -> Result<()> {
    let allowed: bool = cloud.rpc(
        generation,
        "verify_profile_pin",
        json!({"p_profile_id":index,"p_pin":pin}),
    )?;
    if allowed {
        Ok(())
    } else {
        Err(AppError::ProfileLocked)
    }
}

pub fn installed_addons(
    cloud: &CloudClient,
    generation: u64,
    ticket: &HomeTicket,
) -> Result<Vec<InstalledAddon>> {
    let records: Vec<InstalledAddon> = cloud.table(
        generation,
        "addons",
        &[
            ("user_id", format!("eq.{}", ticket.owner)),
            ("profile_id", format!("eq.{}", ticket.addon_profile)),
            ("select", "*".to_owned()),
            ("order", "sort_order.asc".to_owned()),
        ],
    )?;
    Ok(records
        .into_iter()
        .filter(|record| record.enabled)
        .collect())
}

#[derive(Default)]
pub struct HomeOrder {
    positions: HashMap<String, usize>,
    disabled: HashSet<String>,
}

impl HomeOrder {
    pub fn parse(response: Value) -> Result<Self> {
        let mut value = response;
        if let Value::Array(rows) = &value {
            value = rows.first().cloned().unwrap_or(Value::Null);
        }
        if let Some(settings) = value
            .get("settings_json")
            .or_else(|| value.get("settingsJson"))
        {
            value = settings.clone();
        }
        if let Value::String(text) = value {
            value = serde_json::from_str(&text).map_err(|_| AppError::InvalidResponse)?;
        }
        let mut order = Self::default();
        if value.is_null() {
            return Ok(order);
        }
        if let Some(items) = value.get("items") {
            let items = items.as_array().ok_or(AppError::InvalidResponse)?;
            let mut sorted = Vec::new();
            for (index, item) in items.iter().enumerate() {
                let key = item
                    .get("key")
                    .and_then(Value::as_str)
                    .map(str::to_owned)
                    .or_else(|| {
                        Some(format!(
                            "{}_{}_{}",
                            item.get("addon_id")
                                .or_else(|| item.get("addonId"))?
                                .as_str()?,
                            item.get("type")?.as_str()?.to_ascii_lowercase(),
                            item.get("catalog_id")
                                .or_else(|| item.get("catalogId"))?
                                .as_str()?
                        ))
                    });
                if let Some(key) = key {
                    sorted.push((
                        item.get("order")
                            .and_then(Value::as_i64)
                            .unwrap_or(index as i64),
                        index,
                        key.clone(),
                    ));
                    if item.get("enabled") == Some(&Value::Bool(false)) {
                        order.disabled.insert(key);
                    }
                }
            }
            sorted.sort_by_key(|(position, index, _)| (*position, *index));
            for (_, _, key) in sorted {
                let next = order.positions.len();
                order.positions.entry(key).or_insert(next);
            }
        } else {
            for name in [
                "catalog_order_keys",
                "home_catalog_order",
                "catalog_order",
                "order",
            ] {
                if let Some(keys) = value.get(name).and_then(Value::as_array) {
                    for key in keys.iter().filter_map(Value::as_str) {
                        let next = order.positions.len();
                        order.positions.entry(key.to_owned()).or_insert(next);
                    }
                    break;
                }
            }
            for name in [
                "disabled_catalog_keys",
                "hidden_catalog_keys",
                "catalog_disabled_keys",
                "home_catalog_disabled",
                "disabled",
            ] {
                if let Some(keys) = value.get(name).and_then(Value::as_array) {
                    order
                        .disabled
                        .extend(keys.iter().filter_map(Value::as_str).map(str::to_owned));
                    break;
                }
            }
        }
        Ok(order)
    }

    pub fn rows(&self, addons: &[Addon]) -> Vec<HomeRow> {
        let mut rows = Vec::new();
        let mut keys = HashSet::new();
        for addon in addons {
            for catalog in &addon.manifest.catalogs {
                let key = format!(
                    "{}_{}_{}",
                    addon.manifest.id,
                    catalog.kind.to_ascii_lowercase(),
                    catalog.id
                );
                if self.disabled.contains(&key) || !keys.insert(key.clone()) {
                    continue;
                }
                let Some(extras) = catalog.home_extras() else {
                    continue;
                };
                rows.push(HomeRow {
                    key,
                    name: if catalog.name.is_empty() {
                        addon.manifest.name.clone()
                    } else {
                        catalog.name.clone()
                    },
                    addon: addon.clone(),
                    catalog: catalog.clone(),
                    extras,
                    titles: Vec::new(),
                    loaded: false,
                    loading: false,
                    error: None,
                });
            }
        }
        // Stable sorting preserves the source order of newly discovered catalogs.
        rows.sort_by_key(|row| self.positions.get(&row.key).copied().unwrap_or(usize::MAX));
        rows
    }
}

pub fn home_order(cloud: &CloudClient, generation: u64, ticket: &HomeTicket) -> Result<HomeOrder> {
    HomeOrder::parse(cloud.rpc(
        generation,
        "sync_pull_home_catalog_settings",
        json!({"p_profile_id":ticket.profile,"p_platform":"home_catalog_shared"}),
    )?)
}
