//! Account Home ordering, independent of rendering, C buffers and local styles.
use crate::{
    error::{Error, Result},
    json_store::JsonStore,
    model::ProfileId,
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};
use std::{
    collections::{HashMap, HashSet},
    fmt,
    path::Path,
    sync::{Arc, RwLock},
};
use zeroize::Zeroizing;

const RESPONSE_LIMIT: usize = 4 * 1024 * 1024;
const ORDER_NAMES: &[&str] = &[
    "catalog_order_keys",
    "home_catalog_order",
    "catalog_order",
    "order",
];
const HIDDEN_NAMES: &[&str] = &[
    "disabled_catalog_keys",
    "hidden_catalog_keys",
    "catalog_disabled_keys",
    "home_catalog_disabled",
    "disabled",
];

#[derive(Clone, Default, PartialEq, Eq, Serialize, Deserialize)]
pub struct HomeOrder {
    pub keys: Vec<String>,
    pub hidden: HashSet<String>,
    pub hide_unreleased: Option<bool>,
    pub hide_underline: Option<bool>,
}
impl fmt::Debug for HomeOrder {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Legacy row keys can embed configured addon URLs.
        output
            .debug_struct("HomeOrder")
            .field("key_count", &self.keys.len())
            .field("hidden_count", &self.hidden.len())
            .field("hide_unreleased", &self.hide_unreleased)
            .field("hide_underline", &self.hide_underline)
            .finish()
    }
}
#[derive(Deserialize)]
struct OrderItem {
    #[serde(default, alias = "addonId")]
    addon_id: String,
    #[serde(default, rename = "type")]
    media_type: String,
    #[serde(default, alias = "catalogId")]
    catalog_id: String,
    #[serde(default, alias = "isCollection")]
    is_collection: bool,
    #[serde(default, alias = "collectionId")]
    collection_id: String,
    order: Option<f64>,
    enabled: Option<bool>,
}
impl OrderItem {
    fn key(&self) -> Option<String> {
        if self.is_collection {
            return (!self.collection_id.is_empty())
                .then(|| format!("collection_{}", self.collection_id));
        }
        if self.addon_id.is_empty() || self.media_type.is_empty() || self.catalog_id.is_empty() {
            return None;
        }
        Some(format!(
            "{}_{}_{}",
            self.addon_id,
            self.media_type.to_ascii_lowercase(),
            self.catalog_id
        ))
    }
}

impl HomeOrder {
    fn validate(&self) -> Result<()> {
        if self
            .keys
            .iter()
            .chain(self.hidden.iter())
            .any(|key| key.contains('\0'))
        {
            return Err(Error::InvalidData);
        }
        Ok(())
    }
    /// `None` is an absent configuration; `Some(default)` is an explicit reset.
    pub fn decode(bytes: &[u8]) -> Result<Option<Self>> {
        if bytes.len() > RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        let value: Value = serde_json::from_slice(bytes).map_err(|_| Error::InvalidData)?;
        let record = match &value {
            Value::Object(record) => record,
            Value::Array(records) if records.is_empty() => return Ok(None),
            Value::Array(records) if records.len() == 1 => {
                records[0].as_object().ok_or(Error::InvalidData)?
            }
            _ => return Err(Error::InvalidData),
        };
        let object = match record
            .get("settings_json")
            .or_else(|| record.get("settingsJson"))
        {
            Some(Value::Null) => return Ok(None),
            Some(value) => value.as_object().ok_or(Error::InvalidData)?,
            None => record,
        };
        let mut result = Self::default();
        if let Some(items) = object.get("items") {
            let items: Vec<OrderItem> =
                serde_json::from_value(items.clone()).map_err(|_| Error::InvalidData)?;
            let explicitly_empty = items.is_empty();
            let mut entries: Vec<_> = items
                .into_iter()
                .enumerate()
                .filter_map(|(index, item)| {
                    let key = item.key()?;
                    Some((
                        item.order.unwrap_or(index as f64),
                        key,
                        item.enabled != Some(false),
                    ))
                })
                .collect();
            entries.sort_by(|a, b| a.0.total_cmp(&b.0));
            let mut seen = HashSet::new();
            for (_, key, enabled) in entries {
                if !enabled {
                    result.hidden.insert(key.clone());
                }
                if seen.insert(key.clone()) {
                    result.keys.push(key);
                }
            }
            if !explicitly_empty && result.keys.is_empty() {
                return Ok(None);
            }
        } else {
            let order = string_list(object, ORDER_NAMES)?;
            let hidden = string_list(object, HIDDEN_NAMES)?;
            if order.is_none() && hidden.is_none() {
                return Ok(None);
            }
            let mut seen = HashSet::new();
            result.keys = order
                .unwrap_or_default()
                .into_iter()
                .filter(|key| !key.is_empty() && seen.insert(key.clone()))
                .collect();
            result.hidden = hidden
                .unwrap_or_default()
                .into_iter()
                .filter(|key| !key.is_empty())
                .collect();
        }
        result.hide_unreleased = optional_bool(object, "hide_unreleased_content")?;
        result.hide_underline = optional_bool(object, "hide_catalog_underline")?;
        result.validate()?;
        Ok(Some(result))
    }
    pub fn hides(&self, key: &str, provider_key: &str) -> bool {
        self.hidden.contains(key) || self.hidden.contains(provider_key)
    }
    /// Order known rows first and retain every unknown local row in its order.
    pub fn union(&self, local: &[&str]) -> Vec<usize> {
        let mut indexes: HashMap<&str, Vec<usize>> = HashMap::new();
        for (index, key) in local.iter().enumerate() {
            indexes.entry(key).or_default().push(index);
        }
        let mut used = vec![false; local.len()];
        let mut result = Vec::with_capacity(local.len());
        for key in &self.keys {
            if let Some(index) = indexes
                .get(key.as_str())
                .and_then(|entries| entries.iter().find(|index| !used[**index]))
                .copied()
            {
                used[index] = true;
                result.push(index);
            }
        }
        result.extend(
            used.iter()
                .enumerate()
                .filter_map(|(index, used)| (!used).then_some(index)),
        );
        result
    }
}
fn string_list(object: &Map<String, Value>, names: &[&str]) -> Result<Option<Vec<String>>> {
    names
        .iter()
        .find_map(|name| object.get(*name))
        .map(|value| serde_json::from_value(value.clone()).map_err(|_| Error::InvalidData))
        .transpose()
}
fn optional_bool(object: &Map<String, Value>, name: &str) -> Result<Option<bool>> {
    object
        .get(name)
        .map(|value| value.as_bool().ok_or(Error::InvalidData))
        .transpose()
}

#[derive(Default)]
struct PublishedOrder {
    revision: u64,
    order: Arc<HomeOrder>,
}
#[derive(Default)]
pub struct HomeOrderState {
    published: RwLock<PublishedOrder>,
}
impl HomeOrderState {
    pub fn snapshot(&self) -> (u64, Arc<HomeOrder>) {
        let state = self.published.read().unwrap();
        (state.revision, state.order.clone())
    }
    pub fn apply(&self, mut next: HomeOrder) -> bool {
        let mut state = self.published.write().unwrap();
        // Omitted booleans preserve an accepted preference in this context.
        next.hide_unreleased = next.hide_unreleased.or(state.order.hide_unreleased);
        next.hide_underline = next.hide_underline.or(state.order.hide_underline);
        if *state.order == next {
            return false;
        }
        state.order = Arc::new(next);
        state.revision = state.revision.wrapping_add(1);
        true
    }
    pub fn decode_and_apply(&self, bytes: &[u8]) -> Result<bool> {
        Ok(HomeOrder::decode(bytes)?.is_some_and(|order| self.apply(order)))
    }
    pub fn reset(&self) {
        let mut state = self.published.write().unwrap();
        state.order = Arc::new(HomeOrder::default());
        state.revision = state.revision.wrapping_add(1);
    }
}

#[derive(Serialize, Deserialize)]
struct Envelope {
    version: u32,
    app_id: String,
    account: String,
    profile: ProfileId,
    order: HomeOrder,
}
#[derive(Clone)]
pub struct HomeOrderStore {
    files: JsonStore,
}
impl HomeOrderStore {
    pub fn open(directory: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            files: JsonStore::open(directory, TEST_APP_ID)?,
        })
    }
    fn name(profile: ProfileId) -> String {
        format!("home-order-p{}.json", profile.get())
    }
    pub fn load(&self, account: &str, profile: ProfileId) -> Result<Option<HomeOrder>> {
        let Some(bytes) = self
            .files
            .read(&Self::name(profile), RESPONSE_LIMIT as u64)?
        else {
            return Ok(None);
        };
        let envelope: Envelope = serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
        if envelope.version != 1 || envelope.app_id != TEST_APP_ID {
            return Err(Error::InvalidData);
        }
        if envelope.account != account || envelope.profile != profile {
            return Ok(None);
        }
        envelope.order.validate()?;
        Ok(Some(envelope.order))
    }
    pub fn save(&self, account: &str, profile: ProfileId, order: &HomeOrder) -> Result<()> {
        if account.is_empty() {
            return Err(Error::InvalidData);
        }
        order.validate()?;
        let envelope = Envelope {
            version: 1,
            app_id: TEST_APP_ID.into(),
            account: account.into(),
            profile,
            order: order.clone(),
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&envelope).map_err(|_| Error::InvalidData)?);
        self.files
            .write(&Self::name(profile), &bytes, RESPONSE_LIMIT as u64)
    }
    pub fn clear(&self) -> Result<()> {
        for name in self.files.names()? {
            if name
                .strip_prefix("home-order-p")
                .and_then(|value| value.strip_suffix(".json"))
                .and_then(|value| value.parse::<u32>().ok())
                .and_then(ProfileId::new)
                .is_some()
            {
                self.files.remove(&name)?;
            }
        }
        Ok(())
    }
}
