//! Immutable, complete title data retained by native readers independently.
use crate::{
    addons::ADDON_RESPONSE_LIMIT,
    catalog_scene::SceneTitle,
    catalog_titles::{normalized_rating, CatalogTitle},
    error::{Error, Result},
};
use std::{
    ffi::{c_char, CStr, CString},
    ptr,
    sync::{Arc, Mutex, OnceLock},
};

/// Field selectors belong to the native projection, not the domain model.
#[repr(u32)]
#[derive(Clone, Copy)]
pub enum TitleField {
    Id,
    ImdbId,
    MediaType,
    Name,
    Poster,
    Background,
    Logo,
    GenreText,
    MetadataText,
    Description,
    Country,
    Origin,
    CatalogBackground,
    TmdbBackground,
    TraktBackground,
    LogoLanguage,
    LogoLanguageUrl,
    Classification,
    Director,
    SubscriptionName,
    SubscriptionLogo,
    RentalName,
    RentalLogo,
    PurchaseName,
    PurchaseLogo,
    EpisodeName,
}
pub const TITLE_FIELD_COUNT: usize = TitleField::EpisodeName as usize + 1;
/// Read-only scalar projection; the complete scene record remains Rust-owned.
#[repr(C)]
pub struct TitleInfo {
    pub percentage: u32,
    pub season: u32,
    pub episode: u32,
    pub remaining_minutes: u32,
    pub last_watched_ms: i64,
    pub on_watchlist: i32,
    pub in_collection: i32,
    pub cast_count: usize,
    pub season_count: usize,
}
pub struct TitleSnapshot {
    pub record: SceneTitle,
    native: [CString; TITLE_FIELD_COUNT],
    cast: Vec<[CString; 3]>,
    info: TitleInfo,
}
impl TitleSnapshot {
    pub fn new(title: CatalogTitle, origin: String) -> Self {
        let mut record = SceneTitle::from(title);
        record.origin = origin;
        Self::from_scene(record)
    }
    pub fn from_scene(record: SceneTitle) -> Self {
        let title = &record.metadata;
        let genre = record
            .display
            .genre_text
            .clone()
            .unwrap_or_else(|| title.genre_text());
        let metadata = record
            .display
            .metadata_text
            .clone()
            .unwrap_or_else(|| title.metadata_text());
        let provider = |value: &Option<crate::catalog_scene::CommercialProvider>| {
            value
                .as_ref()
                .map(|provider| [provider.name.clone(), provider.logo.clone()])
                .unwrap_or_default()
        };
        let subscription = provider(&record.availability.subscription);
        let rental = provider(&record.availability.rental);
        let purchase = provider(&record.availability.purchase);
        let catalog_background = if record.artwork.catalog_background.is_empty() {
            title.background.as_str()
        } else {
            record.artwork.catalog_background.as_str()
        };
        let values = [
            title.id.as_str(),
            title.imdb_id.as_deref().unwrap_or(""),
            title.media_type.as_str(),
            title.name.as_str(),
            title.poster.as_str(),
            title.background.as_str(),
            title.logo.as_str(),
            genre.as_str(),
            metadata.as_str(),
            title.description.as_str(),
            title.country.as_str(),
            record.origin.as_str(),
            catalog_background,
            record.artwork.tmdb_background.as_str(),
            record.artwork.trakt_background.as_str(),
            record.artwork.logo_language.as_str(),
            record.artwork.logo_language_url.as_str(),
            record.display.classification.as_str(),
            record.display.director.as_str(),
            subscription[0].as_str(),
            subscription[1].as_str(),
            rental[0].as_str(),
            rental[1].as_str(),
            purchase[0].as_str(),
            purchase[1].as_str(),
            record.resume.episode_name.as_str(),
        ];
        let native = values.map(native_text);
        let cast = record
            .cast
            .iter()
            .map(|member| {
                [
                    member.name.as_str(),
                    member.role.as_str(),
                    member.photo.as_str(),
                ]
                .map(native_text)
            })
            .collect();
        let info = TitleInfo {
            percentage: record.resume.percentage.into(),
            season: record.resume.season,
            episode: record.resume.episode,
            remaining_minutes: record.resume.remaining_minutes,
            last_watched_ms: record.resume.last_watched_ms,
            on_watchlist: record.on_watchlist.into(),
            in_collection: record.in_collection.into(),
            cast_count: record.cast.len(),
            season_count: record.seasons.len(),
        };
        Self {
            record,
            native,
            cast,
            info,
        }
    }
    pub(crate) fn text(&self, field: u32) -> &CStr {
        self.native
            .get(field as usize)
            .map(CString::as_c_str)
            .unwrap_or(c"")
    }
}
pub(crate) fn native_text(value: &str) -> CString {
    CString::new(value.replace('\0', " ")).expect("native display text")
}
pub fn native_title(title: CatalogTitle, origin: String) -> *const TitleSnapshot {
    Arc::into_raw(Arc::new(TitleSnapshot::new(title, origin)))
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
pub unsafe extern "C" fn nuvio_title_decode(
    body: *const c_char,
    media_type: *const c_char,
    origin: *const c_char,
) -> *const TitleSnapshot {
    let result = (|| {
        let body = unsafe { text(body)? };
        if body.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        let value = serde_json::from_str(body).map_err(|_| Error::InvalidData)?;
        let title = CatalogTitle::from_meta(&value, unsafe { text(media_type)? })
            .ok_or(Error::InvalidData)?;
        Ok::<_, Error>(native_title(title, unsafe { text(origin)? }.into()))
    })();
    result.unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_retain(title: *const TitleSnapshot) -> *const TitleSnapshot {
    if !title.is_null() {
        unsafe {
            Arc::increment_strong_count(title);
        }
    }
    title
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_release(title: *const TitleSnapshot) {
    if !title.is_null() {
        drop(unsafe { Arc::from_raw(title) });
    }
}
#[derive(Default)]
struct RetiredTitles {
    current: Vec<Arc<TitleSnapshot>>,
    previous: Vec<Arc<TitleSnapshot>>,
}
fn retired() -> &'static Mutex<RetiredTitles> {
    static RETIRED: OnceLock<Mutex<RetiredTitles>> = OnceLock::new();
    RETIRED.get_or_init(|| Mutex::new(RetiredTitles::default()))
}
/// Transfer a replaced catalog reference to frame retirement, preserving text
/// borrowed by a renderer that started before the update.
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_retire(title: *const TitleSnapshot) {
    if !title.is_null() {
        retired()
            .lock()
            .unwrap()
            .current
            .push(unsafe { Arc::from_raw(title) });
    }
}
#[no_mangle]
pub extern "C" fn nuvio_titles_next_frame() {
    crate::scene_snapshot::rows_next_frame();
    let mut retired = retired().lock().unwrap();
    let next = std::mem::take(&mut retired.current);
    retired.previous = next;
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_text(
    title: *const TitleSnapshot,
    field: u32,
) -> *const c_char {
    unsafe { title.as_ref() }
        .map(|title| title.text(field).as_ptr())
        .unwrap_or(c"".as_ptr())
}
/// A native display buffer is only a projection. Complete values remain owned
/// by Rust; the projection always ends at a UTF-8 boundary.
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_copy_text(
    title: *const TitleSnapshot,
    field: u32,
    output: *mut c_char,
    capacity: usize,
) {
    let value = unsafe { title.as_ref() }
        .map(|title| title.text(field).to_bytes())
        .unwrap_or_default();
    unsafe { copy_projection(value, output, capacity) };
}
pub(crate) unsafe fn copy_projection(value: &[u8], output: *mut c_char, capacity: usize) {
    if output.is_null() || capacity == 0 {
        return;
    }
    let mut length = value.len().min(capacity - 1);
    while length > 0 && value.get(length).is_some_and(|byte| byte & 0xc0 == 0x80) {
        length -= 1;
    }
    unsafe {
        ptr::copy_nonoverlapping(value.as_ptr(), output.cast(), length);
        *output.add(length) = 0;
    }
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_rating(title: *const TitleSnapshot) -> i32 {
    unsafe { title.as_ref() }
        .and_then(|title| title.record.metadata.rating)
        .map(|rating| nuvio_title_rating_value(rating))
        .unwrap_or(0)
}
/// Retained metadata readers use the same conversion until their full loaders
/// move to Rust. Invalid or missing ratings have no visible score.
#[no_mangle]
pub extern "C" fn nuvio_title_rating_value(value: f64) -> i32 {
    normalized_rating(value)
        .map(|value| (value * 10.0).round() as i32)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_tmdb_id(title: *const TitleSnapshot) -> u64 {
    unsafe { title.as_ref() }
        .and_then(|title| title.record.metadata.tmdb_id)
        .unwrap_or(0)
}

#[no_mangle]
pub unsafe extern "C" fn nuvio_title_info(title: *const TitleSnapshot) -> *const TitleInfo {
    unsafe { title.as_ref() }
        .map(|title| &title.info as *const _)
        .unwrap_or(ptr::null())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_cast_text(
    title: *const TitleSnapshot,
    index: usize,
    field: u32,
) -> *const c_char {
    unsafe { title.as_ref() }
        .and_then(|title| title.cast.get(index))
        .and_then(|member| member.get(field as usize))
        .map(|value| value.as_ptr())
        .unwrap_or(c"".as_ptr())
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_cast_tmdb_id(
    title: *const TitleSnapshot,
    index: usize,
) -> u64 {
    unsafe { title.as_ref() }
        .and_then(|title| title.record.cast.get(index))
        .and_then(|member| member.tmdb_id)
        .unwrap_or(0)
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_copy_cast_text(
    title: *const TitleSnapshot,
    index: usize,
    field: u32,
    output: *mut c_char,
    capacity: usize,
) {
    let value = unsafe { title.as_ref() }
        .and_then(|title| title.cast.get(index))
        .and_then(|member| member.get(field as usize))
        .map(|value| value.as_bytes())
        .unwrap_or_default();
    unsafe { copy_projection(value, output, capacity) };
}
#[no_mangle]
pub unsafe extern "C" fn nuvio_title_season(title: *const TitleSnapshot, index: usize) -> u32 {
    unsafe { title.as_ref() }
        .and_then(|title| title.record.seasons.get(index))
        .copied()
        .unwrap_or(0)
}
