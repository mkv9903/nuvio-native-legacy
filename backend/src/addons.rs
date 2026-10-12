//! Typed addon identities and Stremio manifests, without C buffer/count limits.
use crate::{
    error::{Error, Result},
    model::Secret,
};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::fmt;
use url::{Position, Url};

pub const ADDON_RESPONSE_LIMIT: usize = 4 * 1024 * 1024;

/// Configuration secrets can appear anywhere in a provider URL. Debug output
/// is redacted; native consumers must explicitly request the complete value.
#[derive(Clone, Debug)]
pub struct AddonUrl(Secret);
impl PartialEq for AddonUrl {
    fn eq(&self, other: &Self) -> bool {
        self.as_str() == other.as_str()
    }
}
impl Eq for AddonUrl {}
impl std::hash::Hash for AddonUrl {
    fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
        std::hash::Hash::hash(self.as_str(), state);
    }
}
impl AddonUrl {
    pub fn parse(input: &str) -> Result<Self> {
        let input = input.trim();
        if input.chars().any(char::is_control) {
            return Err(Error::InvalidData);
        }
        let converted;
        let input = if input
            .get(..10)
            .is_some_and(|prefix| prefix.eq_ignore_ascii_case("stremio://"))
        {
            converted = format!("https://{}", &input[10..]);
            converted.as_str()
        } else {
            input
        };
        let mut url = Url::parse(input).map_err(|_| Error::InvalidData)?;
        if !matches!(url.scheme(), "http" | "https") || url.host_str().is_none() {
            return Err(Error::InvalidData);
        }
        let mut path = url.path().trim_end_matches('/');
        if path
            .get(path.len().saturating_sub(14)..)
            .is_some_and(|suffix| suffix.eq_ignore_ascii_case("/manifest.json"))
        {
            path = &path[..path.len() - 14];
        }
        let path = path.trim_end_matches('/').to_owned();
        url.set_path(&path);
        url.set_fragment(None);
        if url.query() == Some("") {
            url.set_query(None);
        }
        // A root provider base has no trailing slash, just as an installed base.
        let value = if url.path() == "/" {
            format!(
                "{}{}",
                &url[..Position::BeforePath],
                &url[Position::AfterPath..]
            )
        } else {
            url.to_string()
        };
        Ok(Self(Secret::new(value)))
    }
    pub fn as_str(&self) -> &str {
        self.0.expose()
    }
    pub fn manifest_url(&self) -> Url {
        self.append_path(&["manifest.json"])
            .expect("validated provider URL")
    }
    pub fn resource_url(&self, resource: &str, media_type: &str, id: &str) -> Result<Url> {
        if resource.is_empty()
            || media_type.is_empty()
            || id.is_empty()
            || [resource, media_type, id]
                .iter()
                .any(|part| part.contains('\0'))
        {
            return Err(Error::InvalidData);
        }
        self.append_path(&[resource, media_type, &format!("{id}.json")])
    }
    pub fn resource_url_with_extras(
        &self,
        resource: &str,
        media_type: &str,
        id: &str,
        extras: &[(&str, &str)],
    ) -> Result<Url> {
        let mut url = self.resource_url(resource, media_type, id)?;
        if extras.is_empty() {
            return Ok(url);
        }
        if extras
            .iter()
            .any(|(name, value)| name.is_empty() || name.contains('\0') || value.contains('\0'))
        {
            return Err(Error::InvalidData);
        }
        let encoded = url::form_urlencoded::Serializer::new(String::new())
            .extend_pairs(extras.iter().copied())
            .finish()
            .replace('+', "%20");
        let path = format!(
            "{}/{encoded}.json",
            url.path().strip_suffix(".json").expect("resource suffix")
        );
        url.set_path(&path);
        Ok(url)
    }
    fn append_path(&self, parts: &[&str]) -> Result<Url> {
        let mut url = Url::parse(self.as_str()).map_err(|_| Error::InvalidData)?;
        let mut path = url.path_segments_mut().map_err(|_| Error::InvalidData)?;
        path.pop_if_empty();
        for part in parts {
            path.push(part);
        }
        drop(path);
        Ok(url)
    }
}
impl Serialize for AddonUrl {
    fn serialize<S: Serializer>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error> {
        serializer.serialize_str(self.as_str())
    }
}
impl<'de> Deserialize<'de> for AddonUrl {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> std::result::Result<Self, D::Error> {
        let value = Secret::deserialize(deserializer)?;
        Self::parse(value.expose()).map_err(|_| serde::de::Error::custom("invalid addon URL"))
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct InstalledAddon {
    pub name: String,
    pub url: AddonUrl,
    pub enabled: bool,
}
#[derive(Deserialize)]
struct AccountAddon {
    url: AddonUrl,
    name: Option<String>,
    enabled: Option<bool>,
}
impl InstalledAddon {
    /// An empty array is authoritative. A malformed entry rejects the whole
    /// response, so callers can retain the last confirmed list without pruning.
    pub fn decode_account_list(bytes: &[u8]) -> Result<Vec<Self>> {
        if bytes.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        let wire: Vec<AccountAddon> =
            serde_json::from_slice(bytes).map_err(|_| Error::InvalidData)?;
        Ok(wire
            .into_iter()
            .map(|item| Self {
                name: item
                    .name
                    .filter(|name| !name.is_empty())
                    .unwrap_or_else(|| "Addon".into()),
                url: item.url,
                enabled: item.enabled != Some(false),
            })
            .collect())
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ResourceKind {
    Catalog,
    Stream,
    Subtitles,
    Metadata,
}
impl ResourceKind {
    pub fn name(self) -> &'static str {
        match self {
            Self::Catalog => "catalog",
            Self::Stream => "stream",
            Self::Subtitles => "subtitles",
            Self::Metadata => "meta",
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
#[serde(untagged)]
pub enum Resource {
    Name(String),
    Definition {
        name: String,
        types: Option<Vec<String>>,
        #[serde(rename = "idPrefixes")]
        id_prefixes: Option<Vec<String>>,
    },
}
impl Resource {
    fn name(&self) -> &str {
        match self {
            Self::Name(name) | Self::Definition { name, .. } => name,
        }
    }
    fn accepts(&self, manifest: &Manifest, media_type: &str, id: &str) -> bool {
        let (types, prefixes) = match self {
            Self::Name(_) => (manifest.types.as_deref(), manifest.id_prefixes.as_deref()),
            // Standalone object resources do not inherit root ID prefixes.
            // Omitted prefixes accept all IDs, as specified by Stremio.
            Self::Definition {
                types, id_prefixes, ..
            } => {
                // Full resource declarations have their own types as well as
                // prefixes. Missing types cannot authorize a request.
                let Some(types) = types.as_deref() else {
                    return false;
                };
                (Some(types), id_prefixes.as_deref())
            }
        };
        types.is_none_or(|types| {
            types
                .iter()
                .any(|kind| kind.eq_ignore_ascii_case(media_type))
        }) && prefixes.is_none_or(|prefixes| prefixes.iter().any(|prefix| id.starts_with(prefix)))
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
pub struct CatalogExtra {
    pub name: String,
    #[serde(default, rename = "isRequired")]
    pub required: bool,
    #[serde(default)]
    pub options: Vec<String>,
}
#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
pub struct Catalog {
    pub id: String,
    #[serde(rename = "type")]
    pub media_type: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub extra: Vec<CatalogExtra>,
    #[serde(default, rename = "extraSupported")]
    pub extra_supported: Vec<String>,
    #[serde(default, rename = "extraRequired")]
    pub extra_required: Vec<String>,
}
impl Catalog {
    pub fn searchable(&self) -> bool {
        matches!(
            self.media_type.to_ascii_lowercase().as_str(),
            "movie" | "series" | "anime" | "channel" | "tv"
        ) && (self.extra.iter().any(|extra| extra.name == "search")
            || self.extra_supported.iter().any(|name| name == "search"))
    }
    pub fn requires_search(&self) -> bool {
        self.extra
            .iter()
            .any(|extra| extra.name == "search" && extra.required)
            || self.extra_required.iter().any(|name| name == "search")
    }
    pub fn is_channel(&self) -> bool {
        matches!(
            self.media_type.to_ascii_lowercase().as_str(),
            "channel" | "channels" | "tv" | "live" | "iptv"
        )
    }
    pub fn key(&self, addon_id: &str) -> String {
        format!(
            "{addon_id}_{}_{}",
            self.media_type.to_ascii_lowercase(),
            self.id
        )
    }
    pub fn provider_key(&self, base: &AddonUrl) -> String {
        format!(
            "{}_{}_{}_{}",
            base.as_str(),
            self.media_type.to_ascii_lowercase(),
            self.id,
            self.name
        )
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Deserialize, Serialize)]
#[serde(try_from = "ManifestDocument")]
pub struct Manifest {
    #[serde(default)]
    pub id: String,
    #[serde(default)]
    pub name: String,
    pub resources: Option<Vec<Resource>>,
    pub types: Option<Vec<String>>,
    #[serde(rename = "idPrefixes")]
    pub id_prefixes: Option<Vec<String>>,
    #[serde(default)]
    pub catalogs: Vec<Catalog>,
}

#[derive(Deserialize)]
struct ManifestDocument {
    #[serde(default)]
    id: String,
    #[serde(default)]
    name: String,
    #[serde(default, deserialize_with = "declared_array")]
    resources: Option<Vec<Resource>>,
    types: Option<Vec<String>>,
    #[serde(rename = "idPrefixes")]
    id_prefixes: Option<Vec<String>>,
    #[serde(default, deserialize_with = "declared_array")]
    catalogs: Option<Vec<Catalog>>,
    error: Option<serde::de::IgnoredAny>,
}

// A missing declaration is distinct from an explicit empty array. Null and
// wrong-typed declarations are damaged data, never an instruction to clear.
fn declared_array<'de, D, T>(deserializer: D) -> std::result::Result<Option<Vec<T>>, D::Error>
where
    D: Deserializer<'de>,
    T: Deserialize<'de>,
{
    Vec::deserialize(deserializer).map(Some)
}

impl TryFrom<ManifestDocument> for Manifest {
    type Error = &'static str;

    fn try_from(document: ManifestDocument) -> std::result::Result<Self, Self::Error> {
        if document.error.is_some() || (document.resources.is_none() && document.catalogs.is_none())
        {
            return Err("incomplete or failed addon manifest");
        }
        let result = Self {
            id: document.id,
            name: document.name,
            resources: document.resources,
            types: document.types,
            id_prefixes: document.id_prefixes,
            catalogs: document.catalogs.unwrap_or_default(),
        };
        if result.id.contains('\0')
            || result.catalogs.iter().any(|catalog| {
                catalog.id.is_empty()
                    || catalog.media_type.is_empty()
                    || catalog.id.contains('\0')
                    || catalog.media_type.contains('\0')
            })
        {
            return Err("invalid addon manifest identity");
        }
        Ok(result)
    }
}

impl Manifest {
    pub fn decode(bytes: &[u8]) -> Result<Self> {
        if bytes.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        serde_json::from_slice(bytes).map_err(|_| Error::InvalidData)
    }
    /// None retains optimistic discovery when the manifest omits resources.
    pub fn provides(&self, kind: ResourceKind) -> Option<bool> {
        self.resources.as_ref().map(|resources| {
            resources
                .iter()
                .any(|resource| resource.name().eq_ignore_ascii_case(kind.name()))
        })
    }
    pub fn accepts(&self, kind: ResourceKind, media_type: &str, id: &str) -> Option<bool> {
        self.resources.as_ref().map(|resources| {
            resources.iter().any(|resource| {
                resource.name().eq_ignore_ascii_case(kind.name())
                    && resource.accepts(self, media_type, id)
            })
        })
    }
}

impl fmt::Display for ResourceKind {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(self.name())
    }
}
