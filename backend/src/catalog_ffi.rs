//! Opaque catalog jobs and retained scene views at the native boundary.
use crate::{
    addon_ffi::NativeAddonController,
    addon_snapshot::NativeAddonSnapshot,
    addons::AddonUrl,
    catalog_cache::CatalogCache,
    catalog_runtime::CatalogRuntime,
    controller::AccountController,
    error::{Error, Result},
    provider_http::ProviderHttp,
    scene_backend::{SceneView, SceneWork},
    scene_cache::SceneCache,
    scene_snapshot::SceneSnapshot,
};
use std::{
    collections::HashSet,
    ffi::{c_char, CStr, CString},
    ptr,
    sync::{Arc, Mutex},
};
pub struct NativeCatalogWork {
    pub runtime: Arc<CatalogRuntime>,
    pub work: SceneWork,
    empty: Mutex<HashSet<(AddonUrl, String, String)>>,
}
pub struct NativeSceneView {
    pub runtime: Arc<CatalogRuntime>,
    pub view: Arc<SceneView>,
}
pub struct NativeContentLease {
    runtime: Arc<CatalogRuntime>,
    context: crate::content_context::ContentContext,
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_content_begin(
    runtime: *const CatalogRuntime,
) -> *mut NativeContentLease {
    let Some(runtime) = (unsafe { retain(runtime) }) else {
        return ptr::null_mut();
    };
    runtime
        .content_context()
        .map(|context| Box::into_raw(Box::new(NativeContentLease { runtime, context })))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_content_current(lease: *const NativeContentLease) -> i32 {
    i32::from(
        unsafe { lease.as_ref() }
            .is_some_and(|lease| lease.runtime.content_current(&lease.context)),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_content_end(lease: *mut NativeContentLease) {
    if !lease.is_null() {
        drop(unsafe { Box::from_raw(lease) });
    }
}
unsafe fn text<'a>(value: *const c_char) -> Result<&'a str> {
    if value.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_create(
    account: *const AccountController,
    addons: *const NativeAddonController,
    directory: *const c_char,
) -> *const CatalogRuntime {
    let result = (|| {
        let account = unsafe { account.as_ref() }.ok_or(Error::Configuration)?;
        let addons = unsafe { addons.as_ref() }.ok_or(Error::Configuration)?;
        let directory = unsafe { text(directory)? };
        CatalogRuntime::new(
            Arc::new(account.backend.clone()),
            addons.controller.backend.clone(),
            Arc::new(ProviderHttp::default()),
            Arc::new(CatalogCache::open(directory)?),
            Arc::new(SceneCache::open(directory)?),
        )
    })();
    result
        .map(|runtime| Arc::into_raw(Arc::new(runtime)))
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_destroy(runtime: *const CatalogRuntime) {
    if !runtime.is_null() {
        drop(unsafe { Arc::from_raw(runtime) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_step(runtime: *const CatalogRuntime) {
    if let Some(runtime) = unsafe { runtime.as_ref() } {
        let _ = runtime.step();
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_logout(runtime: *const CatalogRuntime) {
    if let Some(runtime) = unsafe { runtime.as_ref() } {
        runtime.logout();
    }
}
unsafe fn retain<T>(value: *const T) -> Option<Arc<T>> {
    if value.is_null() {
        return None;
    }
    unsafe {
        Arc::increment_strong_count(value);
        Some(Arc::from_raw(value))
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_begin(
    runtime: *const CatalogRuntime,
) -> *const NativeCatalogWork {
    let Some(runtime) = (unsafe { retain(runtime) }) else {
        return ptr::null();
    };
    runtime
        .scenes
        .begin_publish()
        .map(|work| {
            Arc::into_raw(Arc::new(NativeCatalogWork {
                runtime,
                work,
                empty: Mutex::new(HashSet::new()),
            }))
        })
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_work_addons(
    work: *const NativeCatalogWork,
) -> *mut NativeAddonSnapshot {
    unsafe { work.as_ref() }
        .filter(|work| work.runtime.scenes.current(&work.work))
        .map(|work| {
            Box::into_raw(Box::new(NativeAddonSnapshot::new(
                work.work.source.addons.clone(),
            )))
        })
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_work_retain(
    work: *const NativeCatalogWork,
) -> *const NativeCatalogWork {
    unsafe { retain(work) }
        .map(Arc::into_raw)
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_work_release(work: *const NativeCatalogWork) {
    if !work.is_null() {
        drop(unsafe { Arc::from_raw(work) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_work_current(work: *const NativeCatalogWork) -> i32 {
    i32::from(unsafe { work.as_ref() }.is_some_and(|work| work.runtime.scenes.current(&work.work)))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_load(
    work: *const NativeCatalogWork,
    base: *const c_char,
    media_type: *const c_char,
    id: *const c_char,
    origin: *const c_char,
    limit: usize,
    responded: *mut i32,
) -> *const SceneSnapshot {
    if let Some(responded) = unsafe { responded.as_mut() } {
        *responded = 0;
    }
    let result = (|| {
        let work = unsafe { work.as_ref() }.ok_or(Error::Cancelled)?;
        work.runtime.load(
            &work.work,
            unsafe { text(base)? },
            unsafe { text(media_type)? },
            unsafe { text(id)? },
            unsafe { text(origin)? },
            limit,
        )
    })();
    match result {
        Ok(result) => {
            if let Some(work) = unsafe { work.as_ref() } {
                if let (Ok(base), Ok(kind), Ok(id)) = (
                    unsafe { text(base) }.and_then(AddonUrl::parse),
                    unsafe { text(media_type) },
                    unsafe { text(id) },
                ) {
                    let mut empty = work.empty.lock().unwrap_or_else(|e| e.into_inner());
                    let key = (base, kind.to_owned(), id.to_owned());
                    if result.scene.titles.is_empty() {
                        empty.insert(key);
                    } else {
                        empty.remove(&key);
                    }
                }
            }
            if let Some(responded) = unsafe { responded.as_mut() } {
                *responded = result.responded.into();
            }
            Arc::into_raw(Arc::new(result.scene))
        }
        Err(_) => ptr::null(),
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_load_index(
    work: *const NativeCatalogWork,
    addon_index: usize,
    catalog_index: usize,
    limit: usize,
    responded: *mut i32,
) -> *const SceneSnapshot {
    if let Some(responded) = unsafe { responded.as_mut() } {
        *responded = 0;
    }
    let request = (|| {
        let handle = unsafe { work.as_ref() }.ok_or(Error::Cancelled)?;
        let (base, kind, id, origin) =
            handle
                .runtime
                .describe(&handle.work, addon_index, catalog_index)?;
        [base, kind, id, origin]
            .map(|value| CString::new(value).map_err(|_| Error::InvalidData))
            .into_iter()
            .collect::<Result<Vec<_>>>()
    })();
    match request {
        Ok(request) => unsafe {
            nuvio_catalog_load(
                work,
                request[0].as_ptr(),
                request[1].as_ptr(),
                request[2].as_ptr(),
                request[3].as_ptr(),
                limit,
                responded,
            )
        },
        Err(_) => ptr::null(),
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_row_reusable(
    work: *const NativeCatalogWork,
    base: *const c_char,
    media_type: *const c_char,
    id: *const c_char,
) -> i32 {
    let result = (|| {
        let work = unsafe { work.as_ref() }.ok_or(Error::Cancelled)?;
        if !work.runtime.scenes.current(&work.work) {
            return Err(Error::Cancelled);
        }
        let key = (
            AddonUrl::parse(unsafe { text(base)? })?,
            unsafe { text(media_type)? }.to_owned(),
            unsafe { text(id)? }.to_owned(),
        );
        Ok(work
            .runtime
            .row_reusable(&work.work, &key.0, &key.1, &key.2)
            && !work
                .empty
                .lock()
                .unwrap_or_else(|e| e.into_inner())
                .contains(&key))
    })();
    i32::from(result.unwrap_or(false))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_accept(
    work: *const NativeCatalogWork,
    scene: *const SceneSnapshot,
    save: i32,
) -> i32 {
    let result = (|| {
        let work = unsafe { work.as_ref() }.ok_or(Error::Cancelled)?;
        let scene = unsafe { retain(scene) }.ok_or(Error::InvalidData)?;
        work.runtime.scenes.accept(&work.work, scene.clone())?;
        if save != 0 {
            work.runtime.scenes.save(&work.work, &scene)?;
        }
        Ok::<_, Error>(())
    })();
    if let Err(error) = &result {
        if *error != Error::Cancelled {
            eprintln!("[catalogs] scene publication failed: {error}");
        }
    }
    i32::from(result.is_ok())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_revision(runtime: *const CatalogRuntime) -> u64 {
    unsafe { runtime.as_ref() }
        .map(|runtime| runtime.scenes.snapshot().revision)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view(
    runtime: *const CatalogRuntime,
) -> *mut NativeSceneView {
    let Some(runtime) = (unsafe { retain(runtime) }) else {
        return ptr::null_mut();
    };
    Box::into_raw(Box::new(NativeSceneView {
        view: runtime.scenes.snapshot(),
        runtime,
    }))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_release(view: *mut NativeSceneView) {
    if !view.is_null() {
        drop(unsafe { Box::from_raw(view) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_clone(
    view: *const NativeSceneView,
) -> *mut NativeSceneView {
    unsafe { view.as_ref() }
        .map(|view| {
            Box::into_raw(Box::new(NativeSceneView {
                runtime: view.runtime.clone(),
                view: view.view.clone(),
            }))
        })
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_context_current(view: *const NativeSceneView) -> i32 {
    i32::from(
        unsafe { view.as_ref() }
            .is_some_and(|view| view.runtime.scenes.context_current(&view.view)),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_addons(
    view: *const NativeSceneView,
) -> *mut NativeAddonSnapshot {
    unsafe { view.as_ref() }
        .and_then(|view| view.runtime.context_addons(&view.view))
        .map(|snapshot| Box::into_raw(Box::new(NativeAddonSnapshot::new(snapshot))))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_current(view: *const NativeSceneView) -> i32 {
    i32::from(
        unsafe { view.as_ref() }
            .is_some_and(|view| view.runtime.scenes.snapshot_current(&view.view)),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_activation(view: *const NativeSceneView) -> u64 {
    unsafe { view.as_ref() }
        .map(|view| view.view.activation())
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_cached(view: *const NativeSceneView) -> i32 {
    i32::from(unsafe { view.as_ref() }.is_some_and(|view| view.view.from_cache))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_requires_apply(view: *const NativeSceneView) -> i32 {
    i32::from(
        unsafe { view.as_ref() }.is_some_and(|view| view.view.from_cache || view.view.filtered),
    )
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_catalog_view_scene(
    view: *const NativeSceneView,
) -> *const SceneSnapshot {
    unsafe { view.as_ref() }
        .and_then(|view| view.view.scene.clone())
        .map(Arc::into_raw)
        .unwrap_or(ptr::null())
}
