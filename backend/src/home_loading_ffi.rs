//! Opaque Home plans and immutable results for the retained native screens.
use crate::{
    addon_snapshot::NativeAddonSnapshot,
    catalog_ffi::NativeCatalogWork,
    home_loading::{HomePlan, LoadStats},
    scene_snapshot::SceneSnapshot,
};
use std::{
    ffi::{c_char, c_void, CString},
    ptr,
    sync::Arc,
};
pub struct NativeHomePlan {
    plan: HomePlan,
}
#[repr(C)]
#[derive(Default)]
pub struct NativeLoadStats {
    pub requested: usize,
    pub responded: usize,
    pub empty: usize,
    pub failed: usize,
    pub rows: usize,
    pub remaining: usize,
}
impl From<LoadStats> for NativeLoadStats {
    fn from(stats: LoadStats) -> Self {
        Self {
            requested: stats.requested,
            responded: stats.responded,
            empty: stats.empty,
            failed: stats.failed,
            rows: stats.rows,
            remaining: stats.remaining,
        }
    }
}
type PrepareProgress = unsafe extern "C" fn(usize, usize, *mut c_void);
type Publish =
    unsafe extern "C" fn(*const SceneSnapshot, i32, *const NativeLoadStats, *mut c_void) -> i32;
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_prepare(
    work: *const NativeCatalogWork,
    progress: Option<PrepareProgress>,
    context: *mut c_void,
) -> *mut NativeHomePlan {
    let Some(work) = (unsafe { work.as_ref() }) else {
        return ptr::null_mut();
    };
    let address = context as usize;
    work.runtime
        .prepare_home(
            &work.work,
            crate::catalog_order_ffi::state().snapshot().1,
            &|ready, total| {
                if let Some(progress) = progress {
                    unsafe {
                        progress(ready, total, address as *mut c_void);
                    }
                }
            },
        )
        .map(|plan| Box::into_raw(Box::new(NativeHomePlan { plan })))
        .unwrap_or(ptr::null_mut())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_destroy(plan: *mut NativeHomePlan) {
    if !plan.is_null() {
        drop(unsafe { Box::from_raw(plan) });
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_profile(plan: *const NativeHomePlan) -> u32 {
    unsafe { plan.as_ref() }
        .and_then(|plan| plan.plan.source.context.as_ref())
        .map(|context| context.profile.get())
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_count(plan: *const NativeHomePlan) -> usize {
    unsafe { plan.as_ref() }
        .map(|plan| plan.plan.catalogs.len())
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_addons(
    plan: *const NativeHomePlan,
) -> *mut NativeAddonSnapshot {
    unsafe { plan.as_ref() }
        .map(|plan| Box::into_raw(Box::new(NativeAddonSnapshot::new(plan.plan.source.clone()))))
        .unwrap_or(ptr::null_mut())
}
/// Owned strings: configured URLs are projected once per native query rather
/// than replicated for every catalog in a large addon manifest.
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_text(
    plan: *const NativeHomePlan,
    index: usize,
    field: u32,
) -> *mut c_char {
    let Some(plan) = (unsafe { plan.as_ref() }) else {
        return ptr::null_mut();
    };
    let Some(entry) = plan.plan.catalogs.get(index) else {
        return ptr::null_mut();
    };
    let addon = &plan.plan.source.records[entry.addon_index];
    let value = match field {
        0 => entry.key.clone(),
        1 => entry.name.clone(),
        2 => addon.installed.url.as_str().into(),
        3 => entry.media_type.clone(),
        4 => entry.id.clone(),
        5 => addon.installed.name.clone(),
        6 => plan.plan.disable_key(index),
        _ => String::new(),
    };
    CString::new(value.replace('\0', " ")).unwrap().into_raw()
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_flags(plan: *const NativeHomePlan, index: usize) -> u32 {
    unsafe { plan.as_ref() }
        .and_then(|plan| {
            plan.plan.catalogs.get(index).map(|entry| {
                u32::from(entry.searchable)
                    | (u32::from(entry.required_extras) << 1)
                    | (u32::from(entry.channel) << 2)
                    | (u32::from(
                        plan.plan.source.records[entry.addon_index]
                            .installed
                            .enabled,
                    ) << 3)
            })
        })
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_choice(
    plan: *mut NativeHomePlan,
    index: usize,
    position: i64,
    flags: u32,
) {
    if let Some(entry) = unsafe { plan.as_mut() }.and_then(|plan| plan.plan.catalogs.get_mut(index))
    {
        entry.choice.local_position = usize::try_from(position).ok();
        entry.choice.hidden = flags & 1 != 0;
        entry.choice.represented_by_collection = flags & 2 != 0;
        entry.choice.channel_selected = flags & 4 != 0;
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_home_load(
    plan: *mut NativeHomePlan,
    work: *const NativeCatalogWork,
    limit: usize,
    items: usize,
    publish: Option<Publish>,
    context: *mut c_void,
    stats: *mut NativeLoadStats,
) -> i32 {
    let (Some(plan), Some(work), Some(publish)) =
        (unsafe { plan.as_mut() }, unsafe { work.as_ref() }, publish)
    else {
        return 0;
    };
    if plan.plan.source.context != work.work.source.addons.context
        || !work
            .runtime
            .home_source_current(&plan.plan.source, &work.work)
    {
        return 0;
    }
    plan.plan.account_order = crate::catalog_order_ffi::state().snapshot().1;
    let result = plan.plan.load(
        limit,
        &|index| {
            let entry = &plan.plan.catalogs[index];
            let addon = &plan.plan.source.records[entry.addon_index];
            work.runtime.load(
                &work.work,
                addon.installed.url.as_str(),
                &entry.media_type,
                &entry.id,
                &entry.origin,
                items,
            )
        },
        &|| {
            work.runtime
                .home_source_current(&plan.plan.source, &work.work)
        },
        |scene, complete, status| {
            let status = NativeLoadStats::from(status);
            (unsafe { publish(Arc::as_ptr(scene), complete as i32, &status, context) }) != 0
        },
    );
    if let Ok(result) = result {
        if let Some(stats) = unsafe { stats.as_mut() } {
            *stats = result.stats.into();
        }
        1
    } else if result.err() == Some(crate::error::Error::Cancelled) {
        0
    } else {
        -1
    }
}
