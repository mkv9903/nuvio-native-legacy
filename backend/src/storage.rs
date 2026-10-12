use crate::{
    error::{Error, Result},
    model::{ProfileSelection, Session},
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{
    collections::HashSet,
    fs::{self, File, OpenOptions},
    io::{Read, Write},
    path::{Path, PathBuf},
};
use uuid::Uuid;
use zeroize::Zeroizing;

const VERSION: u32 = 1;
const MAX_FILE_BYTES: u64 = 1024 * 1024;
const DIRECTORY: &str = "rust-backend-v1";
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
    directory: PathBuf,
}
impl AccountStore {
    pub fn open(app_directory: impl AsRef<Path>, app_id: &str) -> Result<Self> {
        if app_id != TEST_APP_ID {
            return Err(Error::ProductionData);
        }
        let root = app_directory.as_ref();
        if !is_test_directory(root) {
            return Err(Error::ProductionData);
        }
        fs::create_dir_all(root).map_err(|_| Error::Storage)?;
        let root = fs::canonicalize(root).map_err(|_| Error::Storage)?;
        if !is_test_directory(&root) {
            return Err(Error::ProductionData);
        }
        let directory = root.join(DIRECTORY);
        fs::create_dir_all(&directory).map_err(|_| Error::Storage)?;
        let directory = fs::canonicalize(directory).map_err(|_| Error::Storage)?;
        if directory != root.join(DIRECTORY) {
            return Err(Error::ProductionData);
        }
        Ok(Self { directory })
    }

    pub fn load(&self) -> Result<AccountData> {
        let Some(bytes) = self.read()? else {
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
        if bytes.len() as u64 > MAX_FILE_BYTES {
            return Err(Error::ResponseLimit);
        }
        let temporary = self
            .directory
            .join(format!(".account-{}.tmp", Uuid::new_v4()));
        let result = (|| {
            let mut options = OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut file = options.open(&temporary).map_err(|_| Error::Storage)?;
            file.write_all(&bytes)
                .and_then(|_| file.sync_all())
                .map_err(|_| Error::Storage)?;
            fs::rename(&temporary, self.directory.join(ACCOUNT_FILE))
                .map_err(|_| Error::Storage)?;
            File::open(&self.directory)
                .and_then(|directory| directory.sync_all())
                .map_err(|_| Error::Storage)
        })();
        if result.is_err() {
            let _ = fs::remove_file(&temporary);
        }
        result
    }

    pub fn clear(&self) -> Result<()> {
        if let Err(error) = self.save(&AccountData::default()) {
            let _ = fs::remove_file(self.directory.join(ACCOUNT_FILE));
            return Err(error);
        }
        Ok(())
    }

    fn read(&self) -> Result<Option<Zeroizing<Vec<u8>>>> {
        let mut options = OpenOptions::new();
        options.read(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.custom_flags(libc::O_NOFOLLOW);
        }
        let file = match options.open(self.directory.join(ACCOUNT_FILE)) {
            Ok(file) => file,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(_) => return Err(Error::Storage),
        };
        let mut bytes = Zeroizing::new(Vec::new());
        file.take(MAX_FILE_BYTES + 1)
            .read_to_end(&mut bytes)
            .map_err(|_| Error::Storage)?;
        if bytes.len() as u64 > MAX_FILE_BYTES {
            return Err(Error::ResponseLimit);
        }
        Ok(Some(bytes))
    }
}

fn is_test_directory(path: &Path) -> bool {
    path.file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| name == TEST_APP_ID || name == format!(".{TEST_APP_ID}"))
}
