//! Transitional reads of retained C display overlays into complete Rust scenes.
//! No C record layouts, addresses or buffers are part of the saved model.
use crate::{
    addons::AddonUrl,
    catalog_scene::*,
    catalog_titles::CatalogTitle,
    error::{Error, Result},
    scene_snapshot::{native_scene, RowSnapshot, SceneSnapshot},
    title_snapshot::{nuvio_title_rating, TitleField, TitleSnapshot, TITLE_FIELD_COUNT},
};
use std::{
    ffi::{c_char, CStr},
    ptr, slice,
};

#[repr(C)]
pub struct NativeText {
    pub value: *const c_char,
    pub capacity: usize,
}
#[repr(C)]
pub struct NativeCast {
    pub name: NativeText,
    pub role: NativeText,
    pub photo: NativeText,
    pub tmdb_id: u64,
}
#[repr(C)]
pub struct NativeTitleInput {
    pub source: *const TitleSnapshot,
    /// Complete strings from the native accessor, indexed by TitleField.
    pub text: [*const c_char; TITLE_FIELD_COUNT],
    pub rating: i32,
    pub tmdb_id: u64,
    pub percentage: i32,
    pub season: i32,
    pub episode: i32,
    pub remaining_minutes: i32,
    pub last_watched_ms: i64,
    pub on_watchlist: i32,
    pub in_collection: i32,
    pub cast: *const NativeCast,
    pub cast_count: usize,
    pub cast_capacity: usize,
    pub seasons: *const i32,
    pub season_count: usize,
    pub season_capacity: usize,
}
#[repr(C)]
pub struct NativeRowInput {
    pub source: *const RowSnapshot,
    pub key: *const c_char,
    pub name: *const c_char,
    pub media_type: *const c_char,
    pub addon_url: *const c_char,
    pub catalog_id: *const c_char,
    pub start: usize,
    pub count: usize,
    pub state: u32,
}
unsafe fn text<'a>(value: *const c_char) -> Result<&'a str> {
    if value.is_null() {
        return Err(Error::InvalidData);
    }
    unsafe { CStr::from_ptr(value) }
        .to_str()
        .map_err(|_| Error::InvalidData)
}
unsafe fn items<'a, T>(value: *const T, count: usize) -> Result<&'a [T]> {
    if count == 0 {
        return Ok(&[]);
    }
    if value.is_null() || count > isize::MAX as usize / size_of::<T>().max(1) {
        return Err(Error::InvalidData);
    }
    Ok(unsafe { slice::from_raw_parts(value, count) })
}
unsafe fn projected(value: &NativeText) -> Result<&str> {
    let bytes = unsafe { items(value.value.cast::<u8>(), value.capacity)? };
    let end = bytes
        .iter()
        .position(|byte| *byte == 0)
        .ok_or(Error::InvalidData)?;
    std::str::from_utf8(&bytes[..end]).map_err(|_| Error::InvalidData)
}
fn fitting_prefix(value: &str, capacity: usize) -> &str {
    let mut end = value.len().min(capacity.saturating_sub(1));
    while !value.is_char_boundary(end) {
        end -= 1;
    }
    &value[..end]
}
unsafe fn cast_text(value: &NativeText, original: Option<&str>) -> Result<String> {
    let current = unsafe { projected(value)? };
    let sanitized = original
        .filter(|original| original.contains('\0'))
        .map(|original| original.replace('\0', " "));
    Ok(match original {
        Some(original)
            if current
                == fitting_prefix(sanitized.as_deref().unwrap_or(original), value.capacity) =>
        {
            original
        }
        _ => current,
    }
    .into())
}
fn projected_id(id: Option<u64>) -> u64 {
    id.filter(|id| *id <= libc::c_long::MAX as u64).unwrap_or(0)
}
unsafe fn capture_title(input: &NativeTitleInput) -> Result<SceneTitle> {
    let values = input
        .text
        .map(|value| unsafe { text(value).map(str::to_owned) });
    let mut strings = Vec::with_capacity(TITLE_FIELD_COUNT);
    for value in values {
        strings.push(value?);
    }
    let get = |field: TitleField| strings[field as usize].as_str();
    let source = unsafe { input.source.as_ref() };
    let changed = |field: TitleField| {
        source.is_none_or(|source| {
            strings[field as usize].as_bytes() != source.text(field as u32).to_bytes()
        })
    };
    let mut title = if let Some(source) = source {
        source.record.clone()
    } else {
        SceneTitle::from(CatalogTitle {
            id: String::new(),
            imdb_id: None,
            media_type: String::new(),
            name: String::new(),
            poster: String::new(),
            background: String::new(),
            logo: String::new(),
            genres: Vec::new(),
            release_info: String::new(),
            runtime: String::new(),
            description: String::new(),
            country: String::new(),
            rating: None,
            tmdb_id: None,
        })
    };
    // The provider identity from an existing Rust snapshot is immutable.
    if source.is_none() {
        title.metadata.id = get(TitleField::Id).into();
    }
    let imdb = get(TitleField::ImdbId);
    if source.is_none() {
        title.metadata.imdb_id = imdb.starts_with("tt").then(|| imdb.into());
    }
    macro_rules! overlay {
        ($field:ident,$target:expr) => {
            if changed(TitleField::$field) {
                $target = get(TitleField::$field).into();
            }
        };
    }
    overlay!(MediaType, title.metadata.media_type);
    overlay!(Name, title.metadata.name);
    overlay!(Poster, title.metadata.poster);
    overlay!(Background, title.metadata.background);
    overlay!(Logo, title.metadata.logo);
    overlay!(Description, title.metadata.description);
    overlay!(Country, title.metadata.country);
    overlay!(Origin, title.origin);
    if changed(TitleField::GenreText) {
        title.display.genre_text = Some(get(TitleField::GenreText).into());
    }
    if changed(TitleField::MetadataText) {
        title.display.metadata_text = Some(get(TitleField::MetadataText).into());
    }
    overlay!(Classification, title.display.classification);
    overlay!(Director, title.display.director);
    if !(0..=100).contains(&input.rating) {
        return Err(Error::InvalidData);
    }
    if source.is_none_or(|_| input.rating != unsafe { nuvio_title_rating(input.source) }) {
        title.metadata.rating = (input.rating > 0).then_some(input.rating as f64 / 10.0);
    }
    if source.is_none_or(|source| input.tmdb_id != projected_id(source.record.metadata.tmdb_id)) {
        title.metadata.tmdb_id = (input.tmdb_id > 0).then_some(input.tmdb_id);
    }
    overlay!(CatalogBackground, title.artwork.catalog_background);
    overlay!(TmdbBackground, title.artwork.tmdb_background);
    overlay!(TraktBackground, title.artwork.trakt_background);
    overlay!(LogoLanguage, title.artwork.logo_language);
    overlay!(LogoLanguageUrl, title.artwork.logo_language_url);
    let provider = |name: TitleField, logo: TitleField, original: &Option<CommercialProvider>| {
        if !changed(name) && !changed(logo) {
            return original.clone();
        }
        let (name, logo) = (get(name), get(logo));
        (!name.is_empty() || !logo.is_empty()).then(|| CommercialProvider {
            name: name.into(),
            logo: logo.into(),
        })
    };
    title.availability = Availability {
        subscription: provider(
            TitleField::SubscriptionName,
            TitleField::SubscriptionLogo,
            &title.availability.subscription,
        ),
        rental: provider(
            TitleField::RentalName,
            TitleField::RentalLogo,
            &title.availability.rental,
        ),
        purchase: provider(
            TitleField::PurchaseName,
            TitleField::PurchaseLogo,
            &title.availability.purchase,
        ),
    };
    let number = |value: i32, original: u32| -> Result<u32> {
        let value = u32::try_from(value).map_err(|_| Error::InvalidData)?;
        Ok(
            if source.is_some() && value == original.min(i32::MAX as u32) {
                original
            } else {
                value
            },
        )
    };
    title.resume = ResumeInfo {
        percentage: u8::try_from(input.percentage).map_err(|_| Error::InvalidData)?,
        season: number(input.season, title.resume.season)?,
        episode: number(input.episode, title.resume.episode)?,
        remaining_minutes: number(input.remaining_minutes, title.resume.remaining_minutes)?,
        episode_name: if changed(TitleField::EpisodeName) {
            get(TitleField::EpisodeName).into()
        } else {
            title.resume.episode_name
        },
        last_watched_ms: input.last_watched_ms,
    };
    title.on_watchlist = input.on_watchlist != 0;
    title.in_collection = input.in_collection != 0;
    if input.cast_count > input.cast_capacity || input.season_count > input.season_capacity {
        return Err(Error::InvalidData);
    }
    let cast = unsafe { items(input.cast, input.cast_count)? };
    let mut captured = Vec::with_capacity(cast.len());
    for (index, member) in cast.iter().enumerate() {
        let original = source.and_then(|source| source.record.cast.get(index));
        let tmdb_id =
            if original.is_some_and(|original| member.tmdb_id == projected_id(original.tmdb_id)) {
                original.and_then(|original| original.tmdb_id)
            } else {
                (member.tmdb_id > 0).then_some(member.tmdb_id)
            };
        captured.push(CastMember {
            name: unsafe { cast_text(&member.name, original.map(|member| member.name.as_str()))? },
            role: unsafe { cast_text(&member.role, original.map(|member| member.role.as_str()))? },
            photo: unsafe {
                cast_text(&member.photo, original.map(|member| member.photo.as_str()))?
            },
            tmdb_id,
        });
    }
    let full_projection = source.is_some_and(|source| {
        input.cast_count == source.record.cast.len().min(input.cast_capacity)
    });
    if full_projection {
        for (destination, member) in title.cast.iter_mut().zip(captured) {
            *destination = member;
        }
    } else {
        title.cast = captured;
    }
    let seasons = unsafe { items(input.seasons, input.season_count)? };
    let captured = seasons
        .iter()
        .map(|season| u32::try_from(*season).map_err(|_| Error::InvalidData))
        .collect::<Result<Vec<_>>>()?;
    let full_projection = source.is_some_and(|source| {
        input.season_count == source.record.seasons.len().min(input.season_capacity)
    });
    if full_projection {
        for (destination, season) in title.seasons.iter_mut().zip(captured) {
            if season != (*destination).min(i32::MAX as u32) {
                *destination = season;
            }
        }
    } else {
        title.seasons = captured;
    }
    Ok(title)
}

unsafe fn capture_row(input: &NativeRowInput) -> Result<CatalogRow> {
    let strings = [
        input.key,
        input.name,
        input.media_type,
        input.addon_url,
        input.catalog_id,
    ]
    .map(|value| unsafe { text(value).map(str::to_owned) });
    let [key, name, media_type, addon_url, catalog_id] = strings;
    let (key, name, media_type, addon_url, catalog_id) =
        (key?, name?, media_type?, addon_url?, catalog_id?);
    let source = unsafe { input.source.as_ref() };
    let catalog_source = if addon_url.is_empty() {
        None
    } else {
        let addon = AddonUrl::parse(&addon_url)?;
        let extras = source
            .and_then(|source| source.record.source.as_ref())
            .filter(|source| {
                source.addon == addon
                    && source.catalog_id == catalog_id
                    && source.media_type == media_type
            })
            .map(|source| source.extras.clone())
            .unwrap_or_default();
        Some(CatalogSource {
            addon,
            media_type: media_type.clone(),
            catalog_id,
            extras,
        })
    };
    let state = match input.state {
        0 => CatalogRowState::Loading,
        1 => CatalogRowState::Ready,
        2 => CatalogRowState::ConfirmedEmpty,
        _ => return Err(Error::InvalidData),
    };
    Ok(CatalogRow {
        key,
        name,
        media_type,
        source: catalog_source,
        titles: input.start
            ..input
                .start
                .checked_add(input.count)
                .ok_or(Error::InvalidData)?,
        state,
    })
}

#[derive(Default)]
pub struct SceneCapture {
    scene: CatalogScene,
    failed: bool,
}
#[no_mangle]
pub extern "C" fn nuvio_scene_capture_create() -> *mut SceneCapture {
    Box::into_raw(Box::new(SceneCapture::default()))
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_capture_title(
    capture: *mut SceneCapture,
    input: *const NativeTitleInput,
) -> i32 {
    let Some(capture) = (unsafe { capture.as_mut() }) else {
        return 0;
    };
    let Some(input) = (unsafe { input.as_ref() }) else {
        capture.failed = true;
        return 0;
    };
    if capture.failed {
        return 0;
    }
    match unsafe { capture_title(input) } {
        Ok(title) => {
            capture.scene.titles.push(title);
            1
        }
        Err(_) => {
            capture.failed = true;
            0
        }
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_capture_row(
    capture: *mut SceneCapture,
    input: *const NativeRowInput,
) -> i32 {
    let Some(capture) = (unsafe { capture.as_mut() }) else {
        return 0;
    };
    let Some(input) = (unsafe { input.as_ref() }) else {
        capture.failed = true;
        return 0;
    };
    if capture.failed {
        return 0;
    }
    match unsafe { capture_row(input) } {
        Ok(row) => {
            capture.scene.rows.push(row);
            1
        }
        Err(_) => {
            capture.failed = true;
            0
        }
    }
}
/// Consume the capture, returning one owned complete scene or no scene at all.
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_capture_finish(
    capture: *mut SceneCapture,
) -> *const SceneSnapshot {
    if capture.is_null() {
        return ptr::null();
    }
    let capture = unsafe { Box::from_raw(capture) };
    if capture.failed {
        return ptr::null();
    }
    native_scene(capture.scene).unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_scene_capture_destroy(capture: *mut SceneCapture) {
    if !capture.is_null() {
        drop(unsafe { Box::from_raw(capture) });
    }
}
