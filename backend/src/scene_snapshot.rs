//! Immutable native views of complete Rust scenes restored from private JSON.
use crate::{
    catalog_scene::{CatalogRow, CatalogRowState, CatalogScene},
    content_context::ContentScope,
    error::{Error, Result},
    scene_cache::SceneCache,
    title_snapshot::{native_text, TitleSnapshot},
};
use std::{
    ffi::{c_char, CString},
    ptr,
    sync::Arc,
};

#[repr(u32)]
pub enum RowField {
    Key,
    Name,
    MediaType,
    AddonUrl,
    CatalogId,
    RequestUrl,
}
#[repr(C)]
pub struct RowInfo {
    pub start: usize,
    pub end: usize,
    /// 0=loading, 1=ready, 2=confirmed empty; never inferred from title count.
    pub state: u32,
}
pub struct RowSnapshot {
    pub record: CatalogRow,
    native: [CString; 6],
    info: RowInfo,
}
impl RowSnapshot {
    fn new(record: CatalogRow) -> Result<Self> {
        let request = record
            .source
            .as_ref()
            .map(|source| {
                source
                    .addon
                    .resource_url_with_extras(
                        "catalog",
                        &source.media_type,
                        &source.catalog_id,
                        &source
                            .extras
                            .iter()
                            .map(|(key, value)| (key.as_str(), value.as_str()))
                            .collect::<Vec<_>>(),
                    )
                    .map(|url| url.to_string())
            })
            .transpose()?
            .unwrap_or_default();
        let native = [
            record.key.as_str(),
            record.name.as_str(),
            record.media_type.as_str(),
            record
                .source
                .as_ref()
                .map(|source| source.addon.as_str())
                .unwrap_or_default(),
            record
                .source
                .as_ref()
                .map(|source| source.catalog_id.as_str())
                .unwrap_or_default(),
            request.as_str(),
        ]
        .map(native_text);
        let info = RowInfo {
            start: record.titles.start,
            end: record.titles.end,
            state: match record.state {
                CatalogRowState::Loading => 0,
                CatalogRowState::Ready => 1,
                CatalogRowState::ConfirmedEmpty => 2,
            },
        };
        Ok(Self {
            record,
            native,
            info,
        })
    }
}
pub struct SceneSnapshot {
    pub titles: Vec<Arc<TitleSnapshot>>,
    pub rows: Vec<Arc<RowSnapshot>>,
}
impl SceneSnapshot {
    pub fn new(scene: CatalogScene) -> Result<Self> {
        scene.validate()?;
        let rows = scene
            .rows
            .into_iter()
            .map(|row| RowSnapshot::new(row).map(Arc::new))
            .collect::<Result<_>>()?;
        let titles = scene
            .titles
            .into_iter()
            .map(|title| Arc::new(TitleSnapshot::from_scene(title)))
            .collect();
        Ok(Self { titles, rows })
    }
    pub fn to_scene(&self) -> CatalogScene {
        CatalogScene {
            titles: self
                .titles
                .iter()
                .map(|title| title.record.clone())
                .collect(),
            rows: self.rows.iter().map(|row| row.record.clone()).collect(),
        }
    }
    pub fn restore(
        cache: &SceneCache,
        scope: &ContentScope,
        current: &dyn Fn() -> bool,
    ) -> Result<Option<Self>> {
        let snapshot = cache.load(scope, current)?.map(Self::new).transpose()?;
        // Building native text may outlive the captured account/profile context.
        if !current() {
            return Err(Error::Cancelled);
        }
        Ok(snapshot)
    }
}
pub fn native_scene(scene: CatalogScene) -> Result<*const SceneSnapshot> {
    Ok(Arc::into_raw(Arc::new(SceneSnapshot::new(scene)?)))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_retain(scene: *const SceneSnapshot) -> *const SceneSnapshot {
    if !scene.is_null() {
        unsafe {
            Arc::increment_strong_count(scene);
        }
    }
    scene
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_release(scene: *const SceneSnapshot) {
    if !scene.is_null() {
        drop(unsafe { Arc::from_raw(scene) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_title_count(scene: *const SceneSnapshot) -> usize {
    unsafe { scene.as_ref() }
        .map(|scene| scene.titles.len())
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_row_count(scene: *const SceneSnapshot) -> usize {
    unsafe { scene.as_ref() }
        .map(|scene| scene.rows.len())
        .unwrap_or(0)
}
/// Return a separately owned title reference, valid after the scene is released.
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_title(
    scene: *const SceneSnapshot,
    index: usize,
) -> *const TitleSnapshot {
    unsafe { scene.as_ref() }
        .and_then(|scene| scene.titles.get(index))
        .map(|title| Arc::into_raw(title.clone()))
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_row(
    scene: *const SceneSnapshot,
    index: usize,
) -> *const RowSnapshot {
    unsafe { scene.as_ref() }
        .and_then(|scene| scene.rows.get(index))
        .map(|row| Arc::into_raw(row.clone()))
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_row_retain(row: *const RowSnapshot) -> *const RowSnapshot {
    if !row.is_null() {
        unsafe {
            Arc::increment_strong_count(row);
        }
    }
    row
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_row_release(row: *const RowSnapshot) {
    if !row.is_null() {
        drop(unsafe { Arc::from_raw(row) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_row_text(row: *const RowSnapshot, field: u32) -> *const c_char {
    unsafe { row.as_ref() }
        .and_then(|row| row.native.get(field as usize))
        .map(|value| value.as_ptr())
        .unwrap_or(c"".as_ptr())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_row_info(row: *const RowSnapshot) -> *const RowInfo {
    unsafe { row.as_ref() }
        .map(|row| &row.info as *const _)
        .unwrap_or(ptr::null())
}
