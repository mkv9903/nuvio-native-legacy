//! Fresh standalone data only. No reads of the old C/hybrid app's files.
use crate::{
    error::{AppError, Result},
    APP_ID,
};
use serde::{de::DeserializeOwned, Deserialize, Serialize};
use std::{
    env,
    fs::{self, OpenOptions},
    io::{Read, Write},
    path::PathBuf,
};
use zeroize::Zeroizing;

const STORAGE_VERSION: u32 = 1;
const FILE_LIMIT: u64 = 8 * 1024 * 1024;

#[derive(Clone)]
pub struct DataStore {
    root: PathBuf,
}

#[derive(Serialize, Deserialize)]
struct Envelope<T> {
    version: u32,
    value: T,
}

impl DataStore {
    pub fn discover() -> Result<Self> {
        if let Some(path) = env::var_os("NUVIO_RUST_DATA") {
            return Self::new(PathBuf::from(path));
        }
        #[cfg(feature = "webos")]
        let base = env::current_exe()
            .map_err(|_| AppError::Storage)?
            .parent()
            .ok_or(AppError::Storage)?
            .join(format!(".{APP_ID}"));
        #[cfg(not(feature = "webos"))]
        let base =
            match env::var_os("XDG_DATA_HOME").filter(|p| std::path::Path::new(p).is_absolute()) {
                Some(path) => PathBuf::from(path).join(APP_ID),
                None => PathBuf::from(env::var_os("HOME").ok_or(AppError::Storage)?)
                    .join(".local/share")
                    .join(APP_ID),
            };
        Self::new(base.join("standalone-v1"))
    }

    pub fn new(root: PathBuf) -> Result<Self> {
        fs::create_dir_all(&root).map_err(|_| AppError::Storage)?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            fs::set_permissions(&root, fs::Permissions::from_mode(0o700))
                .map_err(|_| AppError::Storage)?;
        }
        Ok(Self { root })
    }

    fn path(&self, name: &str) -> Result<PathBuf> {
        if name.is_empty()
            || name.len() > 200
            || !name
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || matches!(b, b'-' | b'_' | b'.'))
            || name == "."
            || name == ".."
        {
            return Err(AppError::Storage);
        }
        Ok(self.root.join(name))
    }

    pub fn read<T: DeserializeOwned>(&self, name: &str) -> Result<Option<T>> {
        let path = self.path(name)?;
        let file = match fs::File::open(path) {
            Ok(file) => file,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(_) => return Err(AppError::Storage),
        };
        let mut bytes = Zeroizing::new(Vec::new());
        file.take(FILE_LIMIT + 1)
            .read_to_end(&mut bytes)
            .map_err(|_| AppError::Storage)?;
        if bytes.len() as u64 > FILE_LIMIT {
            return Err(AppError::Storage);
        }
        let envelope: Envelope<T> =
            serde_json::from_slice(&bytes).map_err(|_| AppError::Storage)?;
        if envelope.version != STORAGE_VERSION {
            return Err(AppError::Storage);
        }
        Ok(Some(envelope.value))
    }

    pub fn write<T: Serialize>(&self, name: &str, value: &T) -> Result<()> {
        let destination = self.path(name)?;
        let temporary = self
            .root
            .join(format!(".{name}.{}.tmp", uuid::Uuid::new_v4()));
        let bytes = Zeroizing::new(
            serde_json::to_vec(&Envelope {
                version: STORAGE_VERSION,
                value,
            })
            .map_err(|_| AppError::Storage)?,
        );
        if bytes.len() as u64 > FILE_LIMIT {
            return Err(AppError::Storage);
        }
        let result = (|| {
            let mut options = OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut file = options.open(&temporary).map_err(|_| AppError::Storage)?;
            file.write_all(&bytes).map_err(|_| AppError::Storage)?;
            file.sync_all().map_err(|_| AppError::Storage)?;
            fs::rename(&temporary, destination).map_err(|_| AppError::Storage)?;
            fs::File::open(&self.root)
                .and_then(|dir| dir.sync_all())
                .map_err(|_| AppError::Storage)?;
            Ok(())
        })();
        if result.is_err() {
            let _ = fs::remove_file(temporary);
        }
        result
    }

    pub fn remove(&self, name: &str) -> Result<()> {
        match fs::remove_file(self.path(name)?) {
            Ok(()) => fs::File::open(&self.root)
                .and_then(|dir| dir.sync_all())
                .map_err(|_| AppError::Storage),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
            Err(_) => Err(AppError::Storage),
        }
    }
}
