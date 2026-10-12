//! Complete catalog title values, independent of native display buffers.
use crate::{
    addons::ADDON_RESPONSE_LIMIT,
    error::{Error, Result},
};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::fmt;

#[derive(Clone, PartialEq, Serialize, Deserialize)]
pub struct CatalogTitle {
    pub id: String,
    pub imdb_id: Option<String>,
    pub media_type: String,
    pub name: String,
    pub poster: String,
    pub background: String,
    pub logo: String,
    pub genres: Vec<String>,
    pub release_info: String,
    pub runtime: String,
    pub description: String,
    pub country: String,
    pub rating: Option<f64>,
    pub tmdb_id: Option<u64>,
}
impl fmt::Debug for CatalogTitle {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Art URLs can contain private addon/service configuration.
        output
            .debug_struct("CatalogTitle")
            .field("id_bytes", &self.id.len())
            .field("name_bytes", &self.name.len())
            .field("has_imdb_id", &self.imdb_id.is_some())
            .field("genre_count", &self.genres.len())
            .finish_non_exhaustive()
    }
}
fn text<'a>(value: &'a Value, key: &str) -> Option<&'a str> {
    value.get(key)?.as_str()
}
fn nested_text<'a>(value: &'a Value, key: &str) -> Option<&'a str> {
    if let Some(text) = text(value, key) {
        return Some(text);
    }
    match value {
        Value::Object(fields) => fields.values().find_map(|value| nested_text(value, key)),
        Value::Array(items) => items.iter().find_map(|value| nested_text(value, key)),
        _ => None,
    }
}
fn image(value: &Value, key: &str) -> String {
    text(value, key)
        .or_else(|| nested_text(value, key))
        .filter(|value| !value.contains('\0'))
        .unwrap_or_default()
        .into()
}
fn number(value: &Value) -> Option<f64> {
    value
        .as_f64()
        .or_else(|| value.as_str()?.parse().ok())
        .filter(|number| number.is_finite())
}
fn safe_background(input: &str) -> String {
    if url::Url::parse(input)
        .ok()
        .is_some_and(|url| url.host_str() == Some("image.tmdb.org"))
    {
        input.replacen("/t/p/original/", "/t/p/w1280/", 1)
    } else {
        input.into()
    }
}
impl CatalogTitle {
    pub fn from_meta(value: &Value, catalog_type: &str) -> Option<Self> {
        let name = text(value, "name")?.to_owned();
        let poster = image(value, "poster");
        if name.trim().is_empty() || poster.trim().is_empty() {
            return None;
        }
        let imdb_id = text(value, "imdb_id")
            .filter(|id| !id.is_empty() && !id.contains('\0'))
            .map(str::to_owned);
        // Keep the provider's identity for its requests; IMDb is a separate hint.
        let id = text(value, "id")
            .filter(|id| !id.is_empty())
            .or(imdb_id.as_deref())?;
        if id.contains('\0') {
            return None;
        }
        let media_type = text(value, "type")
            .filter(|kind| matches!(*kind, "movie" | "series"))
            .unwrap_or(catalog_type)
            .to_owned();
        let background = image(value, "background");
        let background = if background.is_empty() {
            poster.clone()
        } else {
            safe_background(&background)
        };
        let mut logo = image(value, "logo");
        let path = logo.split(['?', '#']).next().unwrap_or_default();
        if logo == poster || path.to_ascii_lowercase().ends_with(".svg") {
            logo.clear();
        }
        let genres = value
            .get("genres")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .filter_map(Value::as_str)
            .map(str::to_owned)
            .collect();
        let rating = value
            .get("imdbRating")
            .and_then(number)
            .filter(|value| (0.0..=100.0).contains(value))
            .map(|value| if value > 10.0 { value / 10.0 } else { value });
        let tmdb_id = if matches!(media_type.as_str(), "movie" | "series") {
            value
                .get("moviedb_id")
                .and_then(|value| value.as_u64().or_else(|| value.as_str()?.parse().ok()))
                .filter(|id| *id > 0)
        } else {
            None
        };
        Some(Self {
            id: id.into(),
            imdb_id,
            media_type,
            name,
            poster,
            background,
            logo,
            genres,
            release_info: text(value, "releaseInfo").unwrap_or_default().into(),
            runtime: text(value, "runtime").unwrap_or_default().into(),
            description: text(value, "description").unwrap_or_default().into(),
            country: text(value, "country").unwrap_or_default().into(),
            rating,
            tmdb_id,
        })
    }
    pub fn genre_text(&self) -> String {
        let label = match self.media_type.as_str() {
            "series" => "TV Show",
            "channel" | "tv" => "Channel",
            _ => "Movie",
        };
        std::iter::once(label)
            .chain(self.genres.iter().take(2).map(String::as_str))
            .collect::<Vec<_>>()
            .join("  ·  ")
    }
    pub fn metadata_text(&self) -> String {
        let release = self.release_info.split('–').next().unwrap_or_default();
        let release = if release
            .as_bytes()
            .get(..4)
            .is_some_and(|year| year.iter().all(u8::is_ascii_digit))
            && release.as_bytes().get(4) == Some(&b'-')
        {
            &release[..4]
        } else {
            release
        };
        let runtime = self.runtime.trim();
        let digits = runtime.bytes().take_while(u8::is_ascii_digit).count();
        let normalized;
        let runtime = if matches!(runtime[digits..].trim(), "m" | "min")
            && runtime[..digits]
                .parse::<u64>()
                .ok()
                .is_some_and(|number| number > 0)
        {
            normalized = format!(
                "{} min",
                runtime[..digits].parse::<u64>().expect("numeric runtime")
            );
            normalized.as_str()
        } else {
            self.runtime.as_str()
        };
        [release, runtime]
            .into_iter()
            .filter(|part| !part.is_empty())
            .collect::<Vec<_>>()
            .join("  ·  ")
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct CatalogReply {
    pub titles: Vec<CatalogTitle>,
    pub skipped: usize,
}
impl CatalogReply {
    pub fn decode(bytes: &[u8], catalog_type: &str) -> Result<Self> {
        Self::decode_with_limit(bytes, catalog_type, None)
    }
    /// Borrow each raw entry, materializing one JSON object at a time. Large
    /// catalogs never require a second full JSON object tree in TV memory.
    /// A renderer quota is independent of domain field lengths and list limits.
    pub fn decode_with_limit(
        bytes: &[u8],
        catalog_type: &str,
        limit: Option<usize>,
    ) -> Result<Self> {
        if bytes.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        if catalog_type.is_empty() || catalog_type.contains('\0') || limit == Some(0) {
            return Err(Error::InvalidData);
        }
        #[derive(Deserialize)]
        struct Wire<'a> {
            #[serde(borrow)]
            metas: Vec<&'a serde_json::value::RawValue>,
            error: Option<Value>,
        }
        let wire: Wire = serde_json::from_slice(bytes).map_err(|_| Error::InvalidData)?;
        if wire.error.is_some() {
            return Err(Error::InvalidData);
        }
        let count = wire.metas.len();
        let mut titles = Vec::new();
        let mut skipped = 0;
        for raw in wire.metas {
            if limit.is_some_and(|limit| titles.len() >= limit) {
                break;
            }
            let value: Value = match serde_json::from_str(raw.get()) {
                Ok(value) => value,
                Err(_) => {
                    skipped += 1;
                    continue;
                }
            };
            if let Some(title) = CatalogTitle::from_meta(&value, catalog_type) {
                if limit.is_none_or(|limit| titles.len() < limit) {
                    titles.push(title);
                }
            } else {
                skipped += 1;
            }
        }
        if count > 0 && titles.is_empty() {
            return Err(Error::InvalidData);
        }
        Ok(Self { titles, skipped })
    }
}
