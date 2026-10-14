//! Private atomic JSON files shared by Rust-owned capabilities.
use crate::{
    error::{Error, Result},
    TEST_APP_ID,
};
use std::{
    fs::{self, File, OpenOptions},
    io::{Read, Write},
    path::{Path, PathBuf},
};
use uuid::Uuid;
use zeroize::Zeroizing;

#[derive(Clone)]
pub(crate) struct JsonStore {
    directory: PathBuf,
}
impl JsonStore {
    pub fn open(root: impl AsRef<Path>, app_id: &str) -> Result<Self> {
        if app_id != TEST_APP_ID || !is_test_directory(root.as_ref()) {
            return Err(Error::ProductionData);
        }
        fs::create_dir_all(root.as_ref()).map_err(|_| Error::Storage)?;
        let root = fs::canonicalize(root.as_ref()).map_err(|_| Error::Storage)?;
        if !is_test_directory(&root) {
            return Err(Error::ProductionData);
        }
        let expected = root.join("rust-backend-v1");
        fs::create_dir_all(&expected).map_err(|_| Error::Storage)?;
        let directory = fs::canonicalize(&expected).map_err(|_| Error::Storage)?;
        if directory != expected {
            return Err(Error::ProductionData);
        }
        Ok(Self { directory })
    }
    fn path(&self, name: &str) -> Result<PathBuf> {
        if name.is_empty()
            || name.starts_with('.')
            || !name
                .bytes()
                .all(|c| c.is_ascii_alphanumeric() || c == b'-' || c == b'.')
        {
            return Err(Error::Storage);
        }
        Ok(self.directory.join(name))
    }
    pub fn read(&self, name: &str, limit: u64) -> Result<Option<Zeroizing<Vec<u8>>>> {
        let mut options = OpenOptions::new();
        options.read(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options.custom_flags(libc::O_NOFOLLOW);
        }
        let file = match options.open(self.path(name)?) {
            Ok(file) => file,
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(None),
            Err(_) => return Err(Error::Storage),
        };
        let mut bytes = Zeroizing::new(Vec::new());
        file.take(limit + 1)
            .read_to_end(&mut bytes)
            .map_err(|_| Error::Storage)?;
        if bytes.len() as u64 > limit {
            return Err(Error::ResponseLimit);
        }
        Ok(Some(bytes))
    }
    pub fn write(&self, name: &str, bytes: &[u8], limit: u64) -> Result<()> {
        self.write_guarded(name, bytes, limit, &|| true)
    }
    /// Check captured-context validity after the temporary file is complete,
    /// immediately before publishing it. Cancellation preserves the prior file.
    pub fn write_guarded(
        &self,
        name: &str,
        bytes: &[u8],
        limit: u64,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        if bytes.len() as u64 > limit {
            return Err(Error::ResponseLimit);
        }
        let destination = self.path(name)?;
        let temporary = self
            .directory
            .join(format!(".{name}-{}.tmp", Uuid::new_v4()));
        let result = (|| {
            let mut options = OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            let mut file = options.open(&temporary).map_err(|_| Error::Storage)?;
            file.write_all(bytes)
                .and_then(|_| file.sync_all())
                .map_err(|_| Error::Storage)?;
            if !current() {
                return Err(Error::Cancelled);
            }
            fs::rename(&temporary, destination).map_err(|_| Error::Storage)?;
            File::open(&self.directory)
                .and_then(|directory| directory.sync_all())
                .map_err(|_| Error::Storage)
        })();
        if result.is_err() {
            let _ = fs::remove_file(&temporary);
        }
        result
    }
    pub fn remove(&self, name: &str) -> Result<()> {
        match fs::remove_file(self.path(name)?) {
            Ok(()) => Ok(()),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(()),
            Err(_) => Err(Error::Storage),
        }
    }
    pub fn names(&self) -> Result<Vec<String>> {
        fs::read_dir(&self.directory)
            .map_err(|_| Error::Storage)?
            .map(|entry| {
                entry
                    .map_err(|_| Error::Storage)?
                    .file_name()
                    .into_string()
                    .map_err(|_| Error::Storage)
            })
            .collect()
    }
    pub fn with_lock<T>(&self, name: &str, operation: impl FnOnce() -> Result<T>) -> Result<T> {
        let mut options = OpenOptions::new();
        options.read(true).write(true).create(true);
        #[cfg(unix)]
        {
            use std::os::unix::fs::OpenOptionsExt;
            options
                .mode(0o600)
                .custom_flags(libc::O_NOFOLLOW | libc::O_CLOEXEC);
        }
        let file = options.open(self.path(name)?).map_err(|_| Error::Storage)?;
        #[cfg(unix)]
        {
            use std::os::fd::AsRawFd;
            loop {
                if unsafe { libc::flock(file.as_raw_fd(), libc::LOCK_EX) } == 0 {
                    break;
                }
                if std::io::Error::last_os_error().kind() != std::io::ErrorKind::Interrupted {
                    return Err(Error::Storage);
                }
            }
        }
        let result = operation();
        drop(file); // Closing also releases the advisory lock.
        result
    }
}
fn is_test_directory(path: &Path) -> bool {
    path.file_name()
        .and_then(|name| name.to_str())
        .is_some_and(|name| name == TEST_APP_ID || name == format!(".{TEST_APP_ID}"))
}
