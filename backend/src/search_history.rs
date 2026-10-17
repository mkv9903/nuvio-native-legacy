//! Confirmed recent queries, private to the selected account/profile and device.
use crate::{
    content_context::ContentScope,
    error::{Error, Result},
    json_store::JsonStore,
    search_text::{clean_query, history_key},
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{fmt::Write as _, path::Path};
use unicode_normalization::UnicodeNormalization;
use zeroize::Zeroizing;

pub const HISTORY_ENTRIES: usize = 10;
const HISTORY_BYTES: u64 = 1024 * 1024;
#[derive(Clone, Default, Debug, PartialEq, Eq)]
pub struct SearchHistory {
    queries: Vec<String>,
}
impl SearchHistory {
    pub fn queries(&self) -> &[String] {
        &self.queries
    }
    pub fn record(&mut self, query: &str) -> bool {
        let query = clean_query(query);
        if query.nfc().count() < 2 {
            return false;
        }
        let key = history_key(&query);
        self.queries.retain(|previous| history_key(previous) != key);
        self.queries.insert(0, query);
        self.queries.truncate(HISTORY_ENTRIES);
        true
    }
    pub fn remove(&mut self, index: usize) {
        if index < self.queries.len() {
            self.queries.remove(index);
        }
    }
    pub fn clear(&mut self) {
        self.queries.clear();
    }
    fn from_queries(queries: Vec<String>) -> Result<Self> {
        if queries.len() > HISTORY_ENTRIES
            || queries
                .iter()
                .any(|query| clean_query(query) != *query || query.nfc().count() < 2)
        {
            return Err(Error::InvalidData);
        }
        let mut history = Self::default();
        for query in queries.iter().rev() {
            history.record(query);
        }
        if history.queries != queries {
            return Err(Error::InvalidData);
        }
        Ok(history)
    }
}
#[derive(Serialize, Deserialize)]
struct Envelope {
    version: u32,
    app_id: String,
    scope: ContentScope,
    queries: Vec<String>,
}
#[derive(Clone)]
pub struct SearchHistoryStore {
    files: JsonStore,
}
impl SearchHistoryStore {
    pub fn open(directory: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            files: JsonStore::open(directory, TEST_APP_ID)?,
        })
    }
    fn key(scope: &ContentScope) -> Result<String> {
        if scope.member.trim().is_empty()
            || scope.owner.trim().is_empty()
            || scope.member.contains('\0')
            || scope.owner.contains('\0')
        {
            return Err(Error::InvalidData);
        }
        let bytes = Zeroizing::new(serde_json::to_vec(scope).map_err(|_| Error::InvalidData)?);
        let hash = ring::digest::digest(&ring::digest::SHA256, &bytes);
        let mut key = String::with_capacity(64);
        for byte in hash.as_ref() {
            write!(key, "{byte:02x}").expect("writing to String");
        }
        Ok(key)
    }
    pub fn load(&self, scope: &ContentScope, current: &dyn Fn() -> bool) -> Result<SearchHistory> {
        if !current() {
            return Err(Error::Cancelled);
        }
        let bytes = self.files.read(
            &format!("search-history-{}.json", Self::key(scope)?),
            HISTORY_BYTES,
        )?;
        if !current() {
            return Err(Error::Cancelled);
        }
        let Some(bytes) = bytes else {
            return Ok(SearchHistory::default());
        };
        let envelope: Envelope = serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
        if envelope.version != 1 || envelope.app_id != TEST_APP_ID || &envelope.scope != scope {
            return Err(Error::InvalidData);
        }
        SearchHistory::from_queries(envelope.queries)
    }
    pub fn save(
        &self,
        scope: &ContentScope,
        history: &SearchHistory,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        if !current() {
            return Err(Error::Cancelled);
        }
        let key = Self::key(scope)?;
        let envelope = Envelope {
            version: 1,
            app_id: TEST_APP_ID.into(),
            scope: scope.clone(),
            queries: history.queries.clone(),
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&envelope).map_err(|_| Error::Storage)?);
        self.files
            .with_lock(&format!("search-history-lock-{key}"), || {
                if !current() {
                    return Err(Error::Cancelled);
                }
                if history.queries.is_empty() {
                    self.files.remove(&format!("search-history-{key}.json"))
                } else {
                    self.files.write_guarded(
                        &format!("search-history-{key}.json"),
                        &bytes,
                        HISTORY_BYTES,
                        current,
                    )
                }
            })
    }
    pub fn clear(&self) -> Result<()> {
        for name in self.files.names()? {
            if let Some(key) = name
                .strip_prefix("search-history-")
                .and_then(|name| name.strip_suffix(".json"))
                .filter(|key| key.len() == 64 && key.bytes().all(|byte| byte.is_ascii_hexdigit()))
            {
                self.files
                    .with_lock(&format!("search-history-lock-{key}"), || {
                        self.files.remove(&name)
                    })?;
            }
        }
        Ok(())
    }
}
