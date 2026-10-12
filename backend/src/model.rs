use crate::error::{Error, Result};
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::{fmt, num::NonZeroU32};
use zeroize::Zeroizing;

#[derive(Clone)]
pub struct Secret(Zeroizing<String>);
impl Secret {
    pub fn new(value: impl Into<String>) -> Self {
        Self(Zeroizing::new(value.into()))
    }
    pub fn expose(&self) -> &str {
        &self.0
    }
    pub fn is_empty(&self) -> bool {
        self.0.is_empty()
    }
}
impl fmt::Debug for Secret {
    fn fmt(&self, output: &mut fmt::Formatter<'_>) -> fmt::Result {
        output.write_str("[redacted]")
    }
}
impl Serialize for Secret {
    fn serialize<S: Serializer>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error> {
        serializer.serialize_str(self.expose())
    }
}
impl<'de> Deserialize<'de> for Secret {
    fn deserialize<D: Deserializer<'de>>(deserializer: D) -> std::result::Result<Self, D::Error> {
        String::deserialize(deserializer).map(Self::new)
    }
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize, PartialEq, Eq, Hash)]
#[serde(transparent)]
pub struct ProfileId(NonZeroU32);
impl ProfileId {
    pub fn new(value: u32) -> Option<Self> {
        NonZeroU32::new(value).map(Self)
    }
    pub fn get(self) -> u32 {
        self.0.get()
    }
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Session {
    pub access: Secret,
    pub refresh: Secret,
    pub subject: String,
    pub expires_at: Option<u64>,
    pub anonymous: bool,
}

#[derive(Deserialize)]
struct Claims {
    #[serde(default)]
    sub: String,
    #[serde(default)]
    exp: serde_json::Value,
}
impl Session {
    /// Inspect claims to schedule requests; the server verifies the signature.
    pub fn from_tokens(access: Secret, refresh: Secret, anonymous: bool) -> Result<Self> {
        let parts: Vec<_> = access.expose().split('.').collect();
        if parts.len() != 3
            || parts
                .iter()
                .any(|part| part.is_empty() || part.chars().any(char::is_whitespace))
        {
            return Err(Error::InvalidSession);
        }
        let payload = Zeroizing::new(
            URL_SAFE_NO_PAD
                .decode(parts[1])
                .map_err(|_| Error::InvalidSession)?,
        );
        let claims: Claims = serde_json::from_slice(&payload).map_err(|_| Error::InvalidSession)?;
        if !anonymous && claims.sub.is_empty() {
            return Err(Error::InvalidSession);
        }
        Ok(Self {
            access,
            refresh,
            subject: claims.sub,
            expires_at: claims.exp.as_u64().filter(|value| *value > 0),
            anonymous,
        })
    }
    pub fn expires_soon(&self, now: u64) -> bool {
        self.expires_at
            .is_some_and(|expires| expires <= now.saturating_add(30))
    }
    pub fn validate(&self) -> Result<()> {
        let parsed = Self::from_tokens(self.access.clone(), self.refresh.clone(), self.anonymous)?;
        if parsed.subject != self.subject || parsed.expires_at != self.expires_at {
            return Err(Error::InvalidSession);
        }
        Ok(())
    }
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum ProfileLock {
    #[default]
    Unknown,
    Unlocked,
    Locked,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct Profile {
    pub id: ProfileId,
    pub name: String,
    pub color: String,
    pub avatar: String,
    pub avatar_id: String,
    pub background: String,
    pub primary: bool,
    pub lock: ProfileLock,
    pub share_primary_addons: bool,
}

/// Wire names belong to the service protocol; storage uses the domain model.
#[derive(Deserialize)]
pub(crate) struct RemoteProfile {
    profile_index: Option<u32>,
    id: Option<u32>,
    name: Option<String>,
    avatar_color_hex: Option<String>,
    avatar_url: Option<String>,
    avatar_id: Option<String>,
    profile_background_url: Option<String>,
    is_primary: Option<bool>,
    pin_enabled: Option<bool>,
    uses_primary_addons: Option<bool>,
    uses_primary_plugins: Option<bool>,
}
impl TryFrom<RemoteProfile> for Profile {
    type Error = Error;
    fn try_from(value: RemoteProfile) -> Result<Self> {
        let id = value
            .profile_index
            .and_then(ProfileId::new)
            .or_else(|| value.id.and_then(ProfileId::new))
            .ok_or(Error::InvalidData)?;
        Ok(Self {
            id,
            name: value
                .name
                .filter(|name| !name.is_empty())
                .unwrap_or_else(|| format!("Profile {}", id.get())),
            color: value
                .avatar_color_hex
                .filter(|color| !color.is_empty())
                .unwrap_or_else(|| "#1E88E5".to_owned()),
            avatar: value.avatar_url.unwrap_or_default(),
            avatar_id: value.avatar_id.unwrap_or_default(),
            background: value.profile_background_url.unwrap_or_default(),
            primary: value.is_primary.unwrap_or(id.get() == 1),
            lock: match value.pin_enabled {
                Some(true) => ProfileLock::Locked,
                Some(false) => ProfileLock::Unlocked,
                None => ProfileLock::Unknown,
            },
            share_primary_addons: value
                .uses_primary_addons
                .or(value.uses_primary_plugins)
                .unwrap_or(false),
        })
    }
}

#[derive(Clone, Default, Debug, Serialize, Deserialize)]
pub struct ProfileSelection {
    pub profiles: Vec<Profile>,
    pub selected: Option<ProfileId>,
    #[serde(skip)]
    pub confirmed: bool,
}
impl ProfileSelection {
    pub fn active(&self) -> Option<&Profile> {
        self.selected
            .and_then(|id| self.profiles.iter().find(|profile| profile.id == id))
    }
    pub fn addon_profile(&self, force_primary: bool) -> Option<ProfileId> {
        let active = self.active()?;
        if (active.share_primary_addons || force_primary) && !active.primary {
            self.profiles
                .iter()
                .find(|profile| profile.primary)
                .map(|profile| profile.id)
        } else {
            Some(active.id)
        }
    }
    pub fn needs_selection(&self) -> bool {
        !self.confirmed
            && (self.profiles.len() > 1
                || self
                    .profiles
                    .first()
                    .is_some_and(|profile| profile.lock != ProfileLock::Unlocked))
    }
    pub fn can_dismiss(&self) -> bool {
        self.confirmed
            || self
                .active()
                .is_some_and(|profile| profile.lock == ProfileLock::Unlocked)
    }
    pub fn suggested_slot(&self) -> usize {
        self.selected
            .and_then(|id| self.profiles.iter().position(|profile| profile.id == id))
            .unwrap_or(0)
    }
    pub fn select(&mut self, id: ProfileId, pin_verified: bool) -> Result<()> {
        let profile = self
            .profiles
            .iter()
            .find(|profile| profile.id == id)
            .ok_or(Error::InvalidData)?;
        match profile.lock {
            ProfileLock::Unknown => return Err(Error::ProfileUnverified),
            ProfileLock::Locked if !pin_verified => return Err(Error::WrongPin),
            _ => (),
        }
        self.selected = Some(id);
        self.confirmed = true;
        Ok(())
    }
}
