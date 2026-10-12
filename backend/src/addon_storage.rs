//! Fresh private addon cache and pending edits; no legacy-file migration.
use crate::{
    addons::{InstalledAddon, ADDON_RESPONSE_LIMIT},
    content_context::AddonScope,
    error::{Error, Result},
    json_store::JsonStore,
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{fmt::Write, path::Path};
use uuid::Uuid;
use zeroize::Zeroizing;

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct AddonEdit {
    pub id: Uuid,
    pub addons: Vec<InstalledAddon>,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct StoredAddons {
    pub cached: Option<Vec<InstalledAddon>>,
    pub pending: Option<AddonEdit>,
}
pub trait AddonStorage: Send + Sync {
    fn load_state(&self, scope: &AddonScope) -> Result<StoredAddons>;
    fn load_cache(&self, scope: &AddonScope) -> Result<Option<Vec<InstalledAddon>>>;
    fn save_cache(&self, scope: &AddonScope, addons: &[InstalledAddon]) -> Result<()>;
    fn load_edit(&self, scope: &AddonScope) -> Result<Option<AddonEdit>>;
    fn save_edit(&self, scope: &AddonScope, edit: &AddonEdit) -> Result<()>;
    fn acknowledge(&self, scope: &AddonScope, id: Uuid) -> Result<bool>;
    fn clear(&self) -> Result<()>;
}
#[derive(Serialize, Deserialize)]
struct Envelope<T> {
    version: u32,
    app_id: String,
    scope: AddonScope,
    data: T,
}
#[derive(Clone)]
pub struct AddonStore {
    files: JsonStore,
}
impl AddonStore {
    pub fn open(directory: impl AsRef<Path>) -> Result<Self> {
        Ok(Self {
            files: JsonStore::open(directory, TEST_APP_ID)?,
        })
    }
    fn key(scope: &AddonScope) -> Result<String> {
        if scope.member.is_empty() || scope.owner.is_empty() {
            return Err(Error::InvalidData);
        }
        let encoded = serde_json::to_vec(scope).map_err(|_| Error::InvalidData)?;
        let hash = ring::digest::digest(&ring::digest::SHA256, &encoded);
        let mut key = String::with_capacity(64);
        for byte in hash.as_ref() {
            write!(key, "{byte:02x}").expect("writing to String");
        }
        Ok(key)
    }
    fn name(scope: &AddonScope, edit: bool) -> Result<String> {
        Ok(format!(
            "addons-{}-{}.json",
            if edit { "edit" } else { "cache" },
            Self::key(scope)?
        ))
    }
    fn read<T: serde::de::DeserializeOwned>(
        &self,
        scope: &AddonScope,
        edit: bool,
    ) -> Result<Option<T>> {
        let Some(bytes) = self
            .files
            .read(&Self::name(scope, edit)?, ADDON_RESPONSE_LIMIT as u64)?
        else {
            return Ok(None);
        };
        let envelope: Envelope<T> =
            serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
        if envelope.version != 1 || envelope.app_id != TEST_APP_ID || &envelope.scope != scope {
            return Err(Error::InvalidData);
        }
        Ok(Some(envelope.data))
    }
    fn write<T: Serialize + ?Sized>(&self, scope: &AddonScope, edit: bool, data: &T) -> Result<()> {
        let envelope = Envelope {
            version: 1,
            app_id: TEST_APP_ID.into(),
            scope: scope.clone(),
            data,
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&envelope).map_err(|_| Error::InvalidData)?);
        self.files.write(
            &Self::name(scope, edit)?,
            &bytes,
            ADDON_RESPONSE_LIMIT as u64,
        )
    }
    fn locked<T>(&self, scope: &AddonScope, operation: impl FnOnce() -> Result<T>) -> Result<T> {
        self.files
            .with_lock(&format!("addons-lock-{}", Self::key(scope)?), operation)
    }
}
impl AddonStorage for AddonStore {
    fn load_state(&self, scope: &AddonScope) -> Result<StoredAddons> {
        self.locked(scope, || {
            let pending = self.read::<AddonEdit>(scope, true)?;
            if pending.as_ref().is_some_and(|edit| edit.id.is_nil()) {
                return Err(Error::InvalidData);
            }
            let cached = self.read(scope, false)?;
            Ok(StoredAddons { cached, pending })
        })
    }
    fn load_cache(&self, scope: &AddonScope) -> Result<Option<Vec<InstalledAddon>>> {
        self.read(scope, false)
    }
    fn save_cache(&self, scope: &AddonScope, addons: &[InstalledAddon]) -> Result<()> {
        self.locked(scope, || self.write(scope, false, addons))
    }
    fn load_edit(&self, scope: &AddonScope) -> Result<Option<AddonEdit>> {
        self.locked(scope, || {
            let edit = self.read::<AddonEdit>(scope, true)?;
            if edit.as_ref().is_some_and(|edit| edit.id.is_nil()) {
                return Err(Error::InvalidData);
            }
            Ok(edit)
        })
    }
    fn save_edit(&self, scope: &AddonScope, edit: &AddonEdit) -> Result<()> {
        if edit.id.is_nil() {
            return Err(Error::InvalidData);
        }
        self.locked(scope, || self.write(scope, true, edit))
    }
    fn acknowledge(&self, scope: &AddonScope, id: Uuid) -> Result<bool> {
        self.locked(scope, || {
            if let Some(edit) = self
                .read::<AddonEdit>(scope, true)?
                .filter(|edit| edit.id == id)
            {
                // Promote the acknowledged edit before removing its journal.
                // An offline restart must not restore the pre-edit cache.
                self.write(scope, false, &edit.addons)?;
                self.files.remove(&Self::name(scope, true)?)?;
                Ok(true)
            } else {
                Ok(false)
            }
        })
    }
    fn clear(&self) -> Result<()> {
        for name in self.files.names()? {
            let key = name
                .strip_prefix("addons-edit-")
                .or_else(|| name.strip_prefix("addons-cache-"))
                .and_then(|key| key.strip_suffix(".json"));
            if let Some(key) = key
                .filter(|key| key.len() == 64 && key.bytes().all(|byte| byte.is_ascii_hexdigit()))
            {
                self.files
                    .with_lock(&format!("addons-lock-{key}"), || self.files.remove(&name))?;
            }
        }
        // Keep lock inodes: unlinking a live cross-process lock breaks exclusion.
        Ok(())
    }
}
