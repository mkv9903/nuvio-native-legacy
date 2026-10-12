use crate::{
    error::{Error, Result},
    model::{Profile, ProfileId, ProfileLock, ProfileSelection, Secret, Session},
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{
    fs::{self, File, OpenOptions},
    io::{Read, Write},
    path::{Path, PathBuf},
};
use uuid::Uuid;
use zeroize::Zeroizing;

const VERSION: u32 = 1;
const MAX_FILE_BYTES: u64 = 1024 * 1024;
const ACCOUNT_FILE: &str = "account.json";
// Names are external migration identifiers, not names for new application data.
const LEGACY_SESSION: &str = "sessao.txt";
const LEGACY_SELECTED: &str = "perfil.txt";
const LEGACY_PROFILES: &str = "perfis.txt";

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
        let mut ids = std::collections::HashSet::new();
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

/// The directory must be the isolated test application's actual data directory.
/// An explicit app identity and canonical path prevent production migration.
#[derive(Clone)]
pub struct AccountStore {
    directory: PathBuf,
}
impl AccountStore {
    pub fn open(directory: impl AsRef<Path>, app_id: &str) -> Result<Self> {
        if app_id != TEST_APP_ID {
            return Err(Error::ProductionData);
        }
        let path = directory.as_ref();
        if !is_test_directory(path) {
            return Err(Error::ProductionData);
        }
        fs::create_dir_all(path).map_err(|_| Error::Storage)?;
        let directory = fs::canonicalize(path).map_err(|_| Error::Storage)?;
        if !is_test_directory(&directory) {
            return Err(Error::ProductionData);
        }
        Ok(Self { directory })
    }

    pub fn load(&self) -> Result<AccountData> {
        if let Some(bytes) = self.read(ACCOUNT_FILE)? {
            let envelope: Envelope =
                serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
            if envelope.version != VERSION {
                return Err(Error::InvalidData);
            }
            if envelope.app_id != TEST_APP_ID {
                return Err(Error::ProductionData);
            }
            envelope.account.validate()?;
            return Ok(envelope.account);
        }
        let data = self.migrate()?;
        // Writing even an empty envelope prevents old credentials resurfacing.
        self.save(&data)?;
        self.remove_legacy()?;
        Ok(data)
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
        // Remove the import source before clearing current credentials. If disk
        // writes fail, delete the current file too; never retain a logged-in copy.
        let legacy = self.remove_legacy();
        if let Err(error) = self.save(&AccountData::default()) {
            let _ = fs::remove_file(self.directory.join(ACCOUNT_FILE));
            return Err(error);
        }
        legacy
    }

    fn read(&self, name: &str) -> Result<Option<Zeroizing<Vec<u8>>>> {
        let file = match File::open(self.directory.join(name)) {
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

    fn migrate(&self) -> Result<AccountData> {
        let Some(bytes) = self.read(LEGACY_SESSION)? else {
            return Ok(AccountData::default());
        };
        let Ok(text) = std::str::from_utf8(&bytes) else {
            return Ok(AccountData::default());
        };
        let mut lines = text.lines();
        let access = lines.next().unwrap_or_default();
        let refresh = lines.next().unwrap_or_default();
        let anonymous = lines.next().is_some_and(|line| line.trim() == "1");
        let Ok(session) =
            Session::from_tokens(Secret::new(access), Secret::new(refresh), anonymous)
        else {
            return Ok(AccountData::default());
        };
        let mut data = AccountData {
            session: Some(session),
            ..AccountData::default()
        };
        if anonymous {
            return Ok(data);
        }
        if let Some(bytes) = self.read(LEGACY_SELECTED)? {
            data.selection.selected = std::str::from_utf8(&bytes)
                .ok()
                .and_then(|text| text.trim().parse().ok())
                .and_then(ProfileId::new);
        }
        if let Some(bytes) = self.read(LEGACY_PROFILES)? {
            if let Ok(text) = std::str::from_utf8(&bytes) {
                for line in text.lines() {
                    if let Some(profile) = legacy_profile(line) {
                        if !data
                            .selection
                            .profiles
                            .iter()
                            .any(|item| item.id == profile.id)
                        {
                            data.selection.profiles.push(profile);
                        }
                    }
                }
            }
        }
        // The legacy cache contains no account-owner identifier. Obtain it
        // from the service; never substitute the signed-in subject for it.
        Ok(data)
    }

    fn remove_legacy(&self) -> Result<()> {
        let mut result = Ok(());
        for name in [LEGACY_SESSION, LEGACY_SELECTED, LEGACY_PROFILES] {
            match fs::remove_file(self.directory.join(name)) {
                Ok(()) => (),
                Err(error) if error.kind() == std::io::ErrorKind::NotFound => (),
                Err(_) => result = Err(Error::Storage),
            }
        }
        result
    }
}

fn is_test_directory(path: &Path) -> bool {
    let Some(name) = path.file_name().and_then(|name| name.to_str()) else {
        return false;
    };
    (name == TEST_APP_ID || name == format!(".{TEST_APP_ID}"))
        && !path.components().any(|component| {
            matches!(
                component.as_os_str().to_str(),
                Some("space.mkv.nuvio" | ".space.mkv.nuvio")
            )
        })
}

fn legacy_profile(line: &str) -> Option<Profile> {
    let mut fields = line.split('\t');
    let id = ProfileId::new(fields.next()?.parse().ok()?)?;
    let locked = fields.next()?.parse::<u32>().ok()? != 0;
    let primary = fields.next()?.parse::<u32>().ok()? != 0;
    let share = fields.next()?.parse::<u32>().ok()? != 0;
    let color = fields.next()?.to_owned();
    let name = fields.next()?.to_owned();
    let avatar = fields.next()?.to_owned();
    let background = fields.next()?.to_owned();
    if fields.next().is_some() {
        return None;
    }
    Some(Profile {
        id,
        name,
        color,
        avatar,
        avatar_id: String::new(),
        background,
        primary,
        lock: if locked {
            ProfileLock::Locked
        } else {
            ProfileLock::Unlocked
        },
        share_primary_addons: share,
    })
}
