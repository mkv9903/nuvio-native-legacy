//! Versioned Home scenes in private JSON, without C layouts or native pointers.
use crate::{
    catalog_scene::CatalogScene,
    content_context::ContentScope,
    error::{Error, Result},
    json_store::JsonStore,
    TEST_APP_ID,
};
use serde::{Deserialize, Serialize};
use std::{
    fmt::Write as _,
    io::{self, Write},
    path::Path,
};
use zeroize::Zeroizing;

/// Aggregate bytes, independent of title/row counts and domain string lengths.
pub const SCENE_CACHE_LIMIT: u64 = 32 * 1024 * 1024;
#[derive(Serialize)]
struct Envelope<'a> {
    version: u32,
    app_id: &'a str,
    language: &'a str,
    scope: &'a ContentScope,
    scene: &'a CatalogScene,
}
#[derive(Deserialize)]
struct SavedEnvelope {
    version: u32,
    app_id: String,
    language: String,
    scope: ContentScope,
    scene: CatalogScene,
}
struct LimitedBuffer {
    bytes: Zeroizing<Vec<u8>>,
    exceeded: bool,
}
impl Write for LimitedBuffer {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        if bytes.len() as u64 > SCENE_CACHE_LIMIT.saturating_sub(self.bytes.len() as u64) {
            self.exceeded = true;
            return Err(io::Error::other("scene cache byte limit"));
        }
        let required = self.bytes.len() + bytes.len();
        if required > self.bytes.capacity() {
            let capacity = required
                .max(self.bytes.capacity().saturating_mul(2).max(4096))
                .min(SCENE_CACHE_LIMIT as usize);
            let additional = capacity - self.bytes.len();
            self.bytes
                .try_reserve_exact(additional)
                .map_err(|_| io::Error::other("scene cache allocation"))?;
        }
        self.bytes.extend_from_slice(bytes);
        Ok(bytes.len())
    }
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}
#[derive(Clone)]
pub struct SceneCache {
    files: JsonStore,
}
impl SceneCache {
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
        let identity = Zeroizing::new(serde_json::to_vec(scope).map_err(|_| Error::InvalidData)?);
        let hash = ring::digest::digest(&ring::digest::SHA256, &identity);
        let mut key = String::with_capacity(64);
        for byte in hash.as_ref() {
            write!(key, "{byte:02x}").expect("writing to String");
        }
        Ok(key)
    }
    pub fn load(
        &self,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Option<CatalogScene>> {
        if !current() {
            return Err(Error::Cancelled);
        }
        let read = self.files.read(
            &format!("scene-cache-{}.json", Self::key(scope)?),
            SCENE_CACHE_LIMIT,
        );
        if !current() {
            return Err(Error::Cancelled);
        }
        let Some(bytes) = read? else {
            return Ok(None);
        };
        let decoded = (|| {
            let saved: SavedEnvelope =
                serde_json::from_slice(&bytes).map_err(|_| Error::InvalidData)?;
            if saved.version != 1
                || saved.app_id != TEST_APP_ID
                || saved.language != "en"
                || &saved.scope != scope
            {
                return Err(Error::InvalidData);
            }
            saved.scene.validate()?;
            Ok(saved.scene)
        })();
        if !current() {
            return Err(Error::Cancelled);
        }
        decoded.map(Some)
    }
    pub fn save(
        &self,
        scope: &ContentScope,
        scene: &CatalogScene,
        current: &dyn Fn() -> bool,
    ) -> Result<()> {
        if !current() {
            return Err(Error::Cancelled);
        }
        let key = Self::key(scope)?;
        scene.validate()?;
        let mut buffer = LimitedBuffer {
            bytes: Zeroizing::new(Vec::new()),
            exceeded: false,
        };
        serde_json::to_writer(
            &mut buffer,
            &Envelope {
                version: 1,
                app_id: TEST_APP_ID,
                language: "en",
                scope,
                scene,
            },
        )
        .map_err(|_| {
            if buffer.exceeded {
                Error::ResponseLimit
            } else {
                Error::Storage
            }
        })?;
        self.files.with_lock(&format!("scene-lock-{key}"), || {
            if !current() {
                return Err(Error::Cancelled);
            }
            self.files.write_guarded(
                &format!("scene-cache-{key}.json"),
                &buffer.bytes,
                SCENE_CACHE_LIMIT,
                current,
            )
        })
    }
    pub fn clear(&self) -> Result<()> {
        for name in self.files.names()? {
            if let Some(key) = name
                .strip_prefix("scene-cache-")
                .and_then(|name| name.strip_suffix(".json"))
                .filter(|key| key.len() == 64 && key.bytes().all(|byte| byte.is_ascii_hexdigit()))
            {
                self.files
                    .with_lock(&format!("scene-lock-{key}"), || self.files.remove(&name))?;
            }
        }
        Ok(())
    }
}
