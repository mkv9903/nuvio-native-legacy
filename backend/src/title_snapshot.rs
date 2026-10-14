//! Immutable, complete title data retained by native readers independently.
use crate::{
    addons::ADDON_RESPONSE_LIMIT,
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
}
pub struct TitleSnapshot {
    pub title: CatalogTitle,
    pub origin: String,
    native: [CString; 12],
}
impl TitleSnapshot {
    pub fn new(title: CatalogTitle, origin: String) -> Self {
        let genre = title.genre_text();
        let metadata = title.metadata_text();
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
            origin.as_str(),
        ];
        let native =
            values.map(|value| CString::new(value.replace('\0', " ")).expect("display title text"));
        Self {
            title,
            origin,
            native,
        }
    }
    fn text(&self, field: u32) -> &CStr {
        self.native
            .get(field as usize)
            .map(CString::as_c_str)
            .unwrap_or(c"")
    }
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
    if output.is_null() || capacity == 0 {
        return;
    }
    let value = unsafe { title.as_ref() }
        .map(|title| title.text(field).to_bytes())
        .unwrap_or_default();
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
        .and_then(|title| title.title.rating)
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
        .and_then(|title| title.title.tmdb_id)
        .unwrap_or(0)
}
