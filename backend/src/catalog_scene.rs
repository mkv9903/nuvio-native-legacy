//! Complete Home scene data, independent of native record layouts and buffers.
use crate::{
    addons::AddonUrl,
    catalog_titles::CatalogTitle,
    error::{Error, Result},
};
use serde::{Deserialize, Serialize};
use std::{fmt, ops::Range};

#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct ArtworkVariants {
    pub catalog_background: String,
    pub tmdb_background: String,
    pub trakt_background: String,
    pub logo_language: String,
    pub logo_language_url: String,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct TitleDisplay {
    // Retained enrichers may supply a formatted value distinct from catalog meta.
    pub genre_text: Option<String>,
    pub metadata_text: Option<String>,
    pub classification: String,
    pub director: String,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CastMember {
    pub name: String,
    pub role: String,
    pub photo: String,
    pub tmdb_id: Option<u64>,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CommercialProvider {
    pub name: String,
    pub logo: String,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct Availability {
    pub subscription: Option<CommercialProvider>,
    pub rental: Option<CommercialProvider>,
    pub purchase: Option<CommercialProvider>,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct ResumeInfo {
    pub percentage: u8,
    pub season: u32,
    pub episode: u32,
    pub remaining_minutes: u32,
    pub episode_name: String,
    pub last_watched_ms: i64,
}
#[derive(Clone, PartialEq, Serialize, Deserialize)]
pub struct SceneTitle {
    pub metadata: CatalogTitle,
    pub origin: String,
    #[serde(default)]
    pub artwork: ArtworkVariants,
    #[serde(default)]
    pub display: TitleDisplay,
    #[serde(default)]
    pub cast: Vec<CastMember>,
    #[serde(default)]
    pub availability: Availability,
    #[serde(default)]
    pub resume: ResumeInfo,
    #[serde(default)]
    pub seasons: Vec<u32>,
    #[serde(default)]
    pub on_watchlist: bool,
    #[serde(default)]
    pub in_collection: bool,
}
impl From<CatalogTitle> for SceneTitle {
    fn from(metadata: CatalogTitle) -> Self {
        Self {
            metadata,
            origin: String::new(),
            artwork: ArtworkVariants::default(),
            display: TitleDisplay::default(),
            cast: Vec::new(),
            availability: Availability::default(),
            resume: ResumeInfo::default(),
            seasons: Vec::new(),
            on_watchlist: false,
            in_collection: false,
        }
    }
}
impl fmt::Debug for SceneTitle {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output
            .debug_struct("SceneTitle")
            .field("metadata", &self.metadata)
            .field("cast_count", &self.cast.len())
            .field("season_count", &self.seasons.len())
            .finish_non_exhaustive()
    }
}
#[derive(Clone, PartialEq, Serialize, Deserialize)]
pub struct CatalogSource {
    pub addon: AddonUrl,
    pub media_type: String,
    pub catalog_id: String,
    pub extras: Vec<(String, String)>,
}
impl CatalogSource {
    fn validate(&self) -> Result<()> {
        self.addon.resource_url_with_extras(
            "catalog",
            &self.media_type,
            &self.catalog_id,
            &self
                .extras
                .iter()
                .map(|(key, value)| (key.as_str(), value.as_str()))
                .collect::<Vec<_>>(),
        )?;
        Ok(())
    }
}
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum CatalogRowState {
    Loading,
    Ready,
    ConfirmedEmpty,
}
#[derive(Clone, PartialEq, Serialize, Deserialize)]
pub struct CatalogRow {
    pub key: String,
    pub name: String,
    pub media_type: String,
    pub source: Option<CatalogSource>,
    pub titles: Range<usize>,
    pub state: CatalogRowState,
}
#[derive(Clone, Default, PartialEq, Serialize, Deserialize)]
pub struct CatalogScene {
    pub titles: Vec<SceneTitle>,
    pub rows: Vec<CatalogRow>,
}
impl fmt::Debug for CatalogScene {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output
            .debug_struct("CatalogScene")
            .field("title_count", &self.titles.len())
            .field("row_count", &self.rows.len())
            .finish()
    }
}
fn identity(value: &str) -> bool {
    !value.trim().is_empty() && !value.contains('\0')
}
impl CatalogScene {
    /// A cache is one coherent publication. Invalid windows are rejected,
    /// never clamped into another row's titles. Empty scenes are authoritative.
    pub fn validate(&self) -> Result<()> {
        validate_scene(self.titles.iter(), self.rows.iter(), self.titles.len())
    }
}
pub(crate) fn validate_scene<'a>(
    titles: impl Iterator<Item = &'a SceneTitle>,
    rows: impl Iterator<Item = &'a CatalogRow>,
    title_count: usize,
) -> Result<()> {
    for title in titles {
        let metadata = &title.metadata;
        if !identity(&metadata.id)
            || !identity(&metadata.media_type)
            || metadata.imdb_id.as_deref().is_some_and(|id| !identity(id))
            || metadata
                .rating
                .is_some_and(|rating| !rating.is_finite() || !(0.0..=10.0).contains(&rating))
            || metadata.tmdb_id == Some(0)
            || title.resume.percentage > 100
            || title.cast.iter().any(|member| member.tmdb_id == Some(0))
        {
            return Err(Error::InvalidData);
        }
        // Display text can be sanitized at the native boundary. Identities
        // and request URLs cannot change when converted to C strings.
        let artwork = &title.artwork;
        let urls = [
            metadata.poster.as_str(),
            metadata.background.as_str(),
            metadata.logo.as_str(),
            artwork.catalog_background.as_str(),
            artwork.tmdb_background.as_str(),
            artwork.trakt_background.as_str(),
            artwork.logo_language_url.as_str(),
        ];
        if title.origin.contains('\0')
            || urls.iter().any(|url| url.contains('\0'))
            || title.cast.iter().any(|member| member.photo.contains('\0'))
            || [
                &title.availability.subscription,
                &title.availability.rental,
                &title.availability.purchase,
            ]
            .into_iter()
            .flatten()
            .any(|provider| provider.logo.contains('\0'))
        {
            return Err(Error::InvalidData);
        }
    }
    for row in rows {
        if !identity(&row.key)
            || !identity(&row.media_type)
            || row.titles.start > row.titles.end
            || row.titles.end > title_count
            || (row.state == CatalogRowState::ConfirmedEmpty && !row.titles.is_empty())
        {
            return Err(Error::InvalidData);
        }
        if let Some(source) = &row.source {
            source.validate()?;
        }
    }
    Ok(())
}
