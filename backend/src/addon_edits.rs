//! Idempotent local intentions merged into the latest confirmed server list.
use crate::{
    addons::{AddonUrl, InstalledAddon},
    error::{Error, Result},
};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(tag = "action", rename_all = "snake_case", deny_unknown_fields)]
pub enum AddonChange {
    SetEnabled { url: AddonUrl, enabled: bool },
    Install { addon: InstalledAddon },
    Remove { url: AddonUrl },
}
impl AddonChange {
    pub fn url(&self) -> &AddonUrl {
        match self {
            Self::SetEnabled { url, .. } | Self::Remove { url } => url,
            Self::Install { addon } => &addon.url,
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct AddonEdit {
    pub id: Uuid,
    pub changes: Vec<AddonChange>,
}
impl AddonEdit {
    pub fn new(change: AddonChange) -> Self {
        Self {
            id: Uuid::new_v4(),
            changes: vec![change],
        }
    }
    pub fn validate(&self) -> Result<()> {
        if self.id.is_nil() || self.changes.is_empty() {
            return Err(Error::InvalidData);
        }
        Ok(())
    }
    pub fn append(&mut self, change: AddonChange) {
        // Consecutive toggles of the same provider only need the latest value.
        // Install/remove ordering remains significant and is never reordered.
        if let Some(AddonChange::SetEnabled { url, enabled }) = self.changes.last_mut() {
            if let AddonChange::SetEnabled {
                url: next_url,
                enabled: next,
            } = &change
            {
                if url == next_url {
                    *enabled = *next;
                    self.id = Uuid::new_v4();
                    return;
                }
            }
        }
        self.changes.push(change);
        self.id = Uuid::new_v4();
    }
    pub fn merge(&self, latest: &[InstalledAddon]) -> Vec<InstalledAddon> {
        let mut merged = latest.to_vec();
        for change in &self.changes {
            match change {
                AddonChange::SetEnabled { url, enabled } => {
                    // A toggle cannot resurrect a provider removed elsewhere.
                    for addon in merged.iter_mut().filter(|addon| &addon.url == url) {
                        addon.enabled = *enabled;
                    }
                }
                AddonChange::Install { addon } => {
                    if let Some(existing) = merged.iter_mut().find(|entry| entry.url == addon.url) {
                        existing.enabled = addon.enabled;
                    } else {
                        // Preserve remote ordering; explicit new installs append.
                        merged.push(addon.clone());
                    }
                }
                AddonChange::Remove { url } => merged.retain(|addon| &addon.url != url),
            }
        }
        merged
    }
}
