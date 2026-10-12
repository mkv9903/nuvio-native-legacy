use crate::{
    error::{Error, Result},
    json_store::JsonStore,
    model::{ProfileSelection, Session},
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{collections::HashSet, path::Path};
use zeroize::Zeroizing;

const VERSION: u32 = 1;
const MAX_FILE_BYTES: u64 = 1024 * 1024;
const ACCOUNT_FILE: &str = "account.json";

#[derive(Clone, Debug, Default, Serialize, Deserialize)]
pub struct AccountData {
    pub session: Option<Session>,
    pub owner: Option<String>,
    pub selection: ProfileSelection,
}
impl AccountData {
    fn validate(&self) -> Result<()> {
        if let Some(session) = &self.session {
            session.validate()?;
        }
        if self
            .session
            .as_ref()
            .is_none_or(|session| session.anonymous)
            && (self.owner.is_some()
                || !self.selection.profiles.is_empty()
                || self.selection.selected.is_some())
        {
            return Err(Error::InvalidData);
        }
        let mut ids = HashSet::new();
        if self
            .selection
            .profiles
            .iter()
            .any(|profile| !ids.insert(profile.id))
        {
            return Err(Error::InvalidData);
        }
        Ok(())
    }
}

#[derive(Serialize, Deserialize)]
struct Envelope {
    version: u32,
    app_id: String,
    account: AccountData,
}

/// This rewrite reads only its own versioned JSON. C and discarded standalone
/// files are never imported, searched for, or removed.
#[derive(Clone)]
pub struct AccountStore {
    files: JsonStore,
}
impl AccountStore {
    pub fn open(app_directory: impl AsRef<Path>, app_id: &str) -> Result<Self> {
        Ok(Self {
            files: JsonStore::open(app_directory, app_id)?,
        })
    }

    pub fn load(&self) -> Result<AccountData> {
        let Some(bytes) = self.files.read(ACCOUNT_FILE, MAX_FILE_BYTES)? else {
            return Ok(AccountData::default());
        };
        let envelope: Envelope = serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
        if envelope.version != VERSION {
            return Err(Error::InvalidData);
        }
        if envelope.app_id != TEST_APP_ID {
            return Err(Error::ProductionData);
        }
        envelope.account.validate()?;
        Ok(envelope.account)
    }

    pub fn save(&self, data: &AccountData) -> Result<()> {
        data.validate()?;
        let envelope = Envelope {
            version: VERSION,
            app_id: TEST_APP_ID.to_owned(),
            account: data.clone(),
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&envelope).map_err(|_| Error::InvalidData)?);
        self.files.write(ACCOUNT_FILE, &bytes, MAX_FILE_BYTES)
    }

    pub fn clear(&self) -> Result<()> {
        if let Err(error) = self.save(&AccountData::default()) {
            let _ = self.files.remove(ACCOUNT_FILE);
            return Err(error);
        }
        Ok(())
    }
}
