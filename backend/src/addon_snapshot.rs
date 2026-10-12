//! Immutable addon data read by native UI and retained source/subtitle workers.
use crate::{
    addons::{InstalledAddon, Manifest, ResourceKind},
    content_context::ContentContext,
};
use std::{
    ffi::{c_char, CStr, CString},
    ptr,
    sync::Arc,
};

#[derive(Clone, Debug)]
pub struct AddonRecord {
    pub installed: InstalledAddon,
    pub manifest: Option<Arc<Manifest>>,
    pub silent_queries: u32,
}
#[derive(Clone, Debug)]
pub struct AddonSnapshot {
    pub context: Option<ContentContext>,
    pub revision: u64,
    pub records: Arc<[AddonRecord]>,
    pub authoritative: bool,
    pub pending: bool,
}
#[repr(C)]
pub struct CatalogView {
    pub id: *const c_char,
    pub media_type: *const c_char,
    pub name: *const c_char,
    pub searchable: i32,
    pub requires_search: i32,
    pub channel: i32,
}
#[repr(C)]
pub struct AddonView {
    pub name: *const c_char,
    pub base: *const c_char,
    pub id: *const c_char,
    pub enabled: i32,
    pub known: i32,
    pub catalog: i32,
    pub stream: i32,
    pub subtitles: i32,
    pub metadata: i32,
    pub silent_queries: u32,
    pub catalogs: *const CatalogView,
    pub catalog_count: usize,
}
#[repr(C)]
pub struct AddonSnapshotView {
    pub revision: u64,
    pub addons: *const AddonView,
    pub count: usize,
    pub profile: u32,
    pub addon_profile: u32,
    pub authoritative: i32,
    pub pending: i32,
}
struct Projection {
    view: AddonSnapshotView,
    _addons: Box<[AddonView]>,
    _catalogs: Vec<Box<[CatalogView]>>,
    _strings: Vec<CString>,
}
// The projection's pointers refer only to its own boxed arrays and CString
// allocations. They are immutable, and every reader owns an Arc. Native code
// receives const views and must release its snapshot after its last access.
unsafe impl Send for Projection {}
unsafe impl Sync for Projection {}
pub struct NativeAddonSnapshot {
    pub snapshot: AddonSnapshot,
    projection: Arc<Projection>,
}
impl NativeAddonSnapshot {
    pub fn new(snapshot: AddonSnapshot) -> Self {
        let mut strings = Vec::new();
        let mut text = |value: &str| {
            strings.push(CString::new(value.replace('\0', " ")).expect("native addon text"));
            strings.last().unwrap().as_ptr()
        };
        let mut catalogs = Vec::new();
        let addons: Box<[_]> = snapshot
            .records
            .iter()
            .map(|record| {
                let manifest = record.manifest.as_deref();
                let declared: Box<[_]> = manifest
                    .into_iter()
                    .flat_map(|manifest| &manifest.catalogs)
                    .map(|catalog| CatalogView {
                        id: text(&catalog.id),
                        media_type: text(&catalog.media_type),
                        name: text(&catalog.name),
                        searchable: i32::from(catalog.searchable()),
                        requires_search: i32::from(catalog.requires_search()),
                        channel: i32::from(catalog.is_channel()),
                    })
                    .collect::<Vec<_>>()
                    .into_boxed_slice();
                let view = AddonView {
                    name: text(
                        manifest
                            .map(|manifest| manifest.name.as_str())
                            .filter(|name| !name.is_empty())
                            .unwrap_or(if record.installed.name.is_empty() {
                                "Addon"
                            } else {
                                &record.installed.name
                            }),
                    ),
                    base: text(record.installed.url.as_str()),
                    id: text(manifest.map(|manifest| manifest.id.as_str()).unwrap_or("")),
                    enabled: i32::from(record.installed.enabled),
                    known: i32::from(manifest.is_some_and(|manifest| manifest.resources.is_some())),
                    catalog: i32::from(
                        manifest
                            .and_then(|manifest| manifest.provides(ResourceKind::Catalog))
                            .unwrap_or(true),
                    ),
                    stream: i32::from(
                        manifest
                            .and_then(|manifest| manifest.provides(ResourceKind::Stream))
                            .unwrap_or(true),
                    ),
                    subtitles: i32::from(
                        manifest
                            .and_then(|manifest| manifest.provides(ResourceKind::Subtitles))
                            .unwrap_or(true),
                    ),
                    metadata: i32::from(
                        manifest
                            .and_then(|manifest| manifest.provides(ResourceKind::Metadata))
                            .unwrap_or(false),
                    ),
                    silent_queries: record.silent_queries,
                    catalogs: declared.as_ptr(),
                    catalog_count: declared.len(),
                };
                catalogs.push(declared);
                view
            })
            .collect::<Vec<_>>()
            .into_boxed_slice();
        let view = AddonSnapshotView {
            revision: snapshot.revision,
            addons: addons.as_ptr(),
            count: addons.len(),
            profile: snapshot
                .context
                .as_ref()
                .map(|context| context.profile.get())
                .unwrap_or(0),
            addon_profile: snapshot
                .context
                .as_ref()
                .map(|context| context.addons.profile.get())
                .unwrap_or(0),
            authoritative: i32::from(snapshot.authoritative),
            pending: i32::from(snapshot.pending),
        };
        let projection = Arc::new(Projection {
            view,
            _addons: addons,
            _catalogs: catalogs,
            _strings: strings,
        });
        Self {
            snapshot,
            projection,
        }
    }
    pub fn view(&self) -> &AddonSnapshotView {
        &self.projection.view
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_clone(
    snapshot: *const NativeAddonSnapshot,
) -> *mut NativeAddonSnapshot {
    unsafe { snapshot.as_ref() }
        .map(|snapshot| {
            Box::into_raw(Box::new(NativeAddonSnapshot {
                snapshot: snapshot.snapshot.clone(),
                projection: snapshot.projection.clone(),
            }))
        })
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_view(
    snapshot: *const NativeAddonSnapshot,
) -> *const AddonSnapshotView {
    unsafe { snapshot.as_ref() }
        .map(|snapshot| snapshot.view() as *const _)
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_destroy(snapshot: *mut NativeAddonSnapshot) {
    if !snapshot.is_null() {
        drop(unsafe { Box::from_raw(snapshot) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_accepts(
    snapshot: *const NativeAddonSnapshot,
    index: usize,
    kind: i32,
    media_type: *const c_char,
    id: *const c_char,
) -> i32 {
    let Some(snapshot) = (unsafe { snapshot.as_ref() }) else {
        return 0;
    };
    if media_type.is_null() || id.is_null() {
        return 0;
    }
    let Some(record) = snapshot.snapshot.records.get(index) else {
        return 0;
    };
    let (Ok(media_type), Ok(id)) = (
        (unsafe { CStr::from_ptr(media_type) }).to_str(),
        (unsafe { CStr::from_ptr(id) }).to_str(),
    ) else {
        return 0;
    };
    let kind = match kind {
        0 => ResourceKind::Catalog,
        1 => ResourceKind::Stream,
        2 => ResourceKind::Subtitles,
        3 => ResourceKind::Metadata,
        _ => return 0,
    };
    record
        .manifest
        .as_deref()
        .and_then(|manifest| manifest.accepts(kind, media_type, id))
        .map(i32::from)
        .unwrap_or(-1)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_addons_snapshot_url(
    snapshot: *const NativeAddonSnapshot,
    index: usize,
    resource: *const c_char,
    media_type: *const c_char,
    id: *const c_char,
) -> *mut c_char {
    let Some(snapshot) = (unsafe { snapshot.as_ref() }) else {
        return ptr::null_mut();
    };
    let Some(record) = snapshot.snapshot.records.get(index) else {
        return ptr::null_mut();
    };
    if resource.is_null() || media_type.is_null() || id.is_null() {
        return ptr::null_mut();
    }
    let (Ok(resource), Ok(media_type), Ok(id)) = (
        (unsafe { CStr::from_ptr(resource) }).to_str(),
        (unsafe { CStr::from_ptr(media_type) }).to_str(),
        (unsafe { CStr::from_ptr(id) }).to_str(),
    ) else {
        return ptr::null_mut();
    };
    record
        .installed
        .url
        .resource_url(resource, media_type, id)
        .ok()
        .and_then(|url| CString::new(url.to_string()).ok())
        .map(CString::into_raw)
        .unwrap_or(ptr::null_mut())
}
