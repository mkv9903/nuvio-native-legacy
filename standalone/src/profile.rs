//! Profile identity is attached to every asynchronous Home load.
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
#[serde(from = "ProfileWire")]
pub struct Profile {
    pub profile_index: u32,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub is_primary: bool,
    pub uses_primary_addons: bool,
    pub avatar_url: String,
    pub background_url: String,
    pub color_hex: String,
    pub has_pin: bool,
}

#[derive(Deserialize)]
struct ProfileWire {
    profile_index: Option<u32>,
    id: Option<u32>,
    name: Option<String>,
    is_primary: Option<bool>,
    uses_primary_addons: Option<bool>,
    uses_primary_plugins: Option<bool>,
    avatar_url: Option<String>,
    profile_background_url: Option<String>,
    avatar_color_hex: Option<String>,
    pin_enabled: Option<bool>,
}

impl From<ProfileWire> for Profile {
    fn from(wire: ProfileWire) -> Self {
        let index = wire
            .profile_index
            .filter(|v| *v > 0)
            .or(wire.id)
            .unwrap_or(0);
        Self {
            profile_index: index,
            name: wire
                .name
                .filter(|v| !v.is_empty())
                .unwrap_or_else(|| format!("Profile {index}")),
            is_primary: wire.is_primary.unwrap_or(index == 1),
            uses_primary_addons: wire
                .uses_primary_addons
                .or(wire.uses_primary_plugins)
                .unwrap_or(false),
            avatar_url: wire.avatar_url.unwrap_or_default(),
            background_url: wire.profile_background_url.unwrap_or_default(),
            color_hex: wire
                .avatar_color_hex
                .unwrap_or_else(|| "#1E88E5".to_owned()),
            has_pin: wire.pin_enabled.unwrap_or(false),
        }
    }
}

impl Profile {
    pub fn addon_profile(&self, profiles: &[Self]) -> Result<u32, &'static str> {
        if self.profile_index == 0 {
            return Err("Invalid profile index");
        }
        if !self.uses_primary_addons || self.is_primary {
            return Ok(self.profile_index);
        }
        let mut primary = profiles.iter().filter(|p| p.is_primary);
        let first = primary.next().ok_or("Primary profile is missing")?;
        if primary.next().is_some() || first.profile_index == 0 {
            return Err("Primary profile is ambiguous");
        }
        Ok(first.profile_index)
    }
}

#[derive(Clone, PartialEq, Eq)]
pub struct HomeTicket {
    pub owner: String,
    pub profile: u32,
    pub addon_profile: u32,
    generation: u64,
}

/// A new selection owns a fresh collection, even when both profiles share addons.
/// The generation also rejects A's old response after an A -> B -> A switch.
pub struct ProfileHome<T> {
    generation: u64,
    ticket: Option<HomeTicket>,
    pub rows: Vec<T>,
}

impl<T> Default for ProfileHome<T> {
    fn default() -> Self {
        Self {
            generation: 0,
            ticket: None,
            rows: Vec::new(),
        }
    }
}

impl<T> ProfileHome<T> {
    pub fn begin(
        &mut self,
        owner: &str,
        selected: &Profile,
        profiles: &[Profile],
    ) -> Result<HomeTicket, &'static str> {
        if owner.is_empty() {
            return Err("Account owner is missing");
        }
        let addon_profile = selected.addon_profile(profiles)?;
        let generation = self
            .generation
            .checked_add(1)
            .ok_or("Home generation exhausted")?;
        let ticket = HomeTicket {
            owner: owner.to_owned(),
            profile: selected.profile_index,
            addon_profile,
            generation,
        };
        self.generation = generation;
        self.rows.clear();
        self.ticket = Some(ticket.clone());
        Ok(ticket)
    }

    pub fn replace(&mut self, ticket: &HomeTicket, rows: Vec<T>) -> bool {
        if self.ticket.as_ref() != Some(ticket) {
            return false;
        }
        self.rows = rows;
        true
    }

    pub fn clear(&mut self) {
        self.ticket = None;
        self.rows.clear();
    }

    pub fn accepts(&self, ticket: &HomeTicket) -> bool {
        self.ticket.as_ref() == Some(ticket)
    }

    pub fn ticket(&self) -> Option<&HomeTicket> {
        self.ticket.as_ref()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn profiles() -> Vec<Profile> {
        serde_json::from_str(
            r#"[
            {"profile_index":1,"name":"Primary","is_primary":true},
            {"profile_index":2,"name":"Shared","uses_primary_addons":true},
            {"profile_index":3,"name":"Independent","uses_primary_addons":false}
        ]"#,
        )
        .unwrap()
    }

    #[test]
    fn sharing_changes_addon_source_but_preserves_active_profile() {
        let p = profiles();
        let mut home = ProfileHome::<String>::default();
        let t = home.begin("fixture-owner", &p[1], &p).unwrap();
        assert_eq!(t.profile, 2);
        assert_eq!(t.addon_profile, 1);
        assert_eq!(p[2].addon_profile(&p), Ok(3));
    }

    #[test]
    fn switching_shared_profiles_clears_previous_rows() {
        let p = profiles();
        let mut home = ProfileHome::default();
        let a = home.begin("fixture-owner", &p[0], &p).unwrap();
        assert!(home.replace(&a, vec!["Primary catalog"]));
        let b = home.begin("fixture-owner", &p[1], &p).unwrap();
        assert!(home.rows.is_empty());
        assert!(!home.replace(&a, vec!["Late primary catalog"]));
        assert!(home.replace(&b, vec!["Shared profile catalog"]));
        assert_eq!(home.rows, ["Shared profile catalog"]);
    }

    #[test]
    fn returning_to_profile_rejects_its_previous_load() {
        let p = profiles();
        let mut home = ProfileHome::default();
        let old_a = home.begin("fixture-owner", &p[0], &p).unwrap();
        home.begin("fixture-owner", &p[2], &p).unwrap();
        let new_a = home.begin("fixture-owner", &p[0], &p).unwrap();
        assert!(!home.replace(&old_a, vec!["stale"]));
        assert!(home.replace(&new_a, vec!["current"]));
    }

    #[test]
    fn account_switch_and_logout_reject_outstanding_loads() {
        let p = profiles();
        let mut home = ProfileHome::default();
        let old = home.begin("fixture-owner-a", &p[0], &p).unwrap();
        let current = home.begin("fixture-owner-b", &p[0], &p).unwrap();
        assert!(!home.replace(&old, vec!["wrong account"]));
        assert!(home.replace(&current, vec!["right account"]));
        home.clear();
        assert!(!home.replace(&current, vec!["after logout"]));
        assert!(home.rows.is_empty());
    }

    #[test]
    fn primary_profile_is_taken_from_account_data() {
        let mut p = profiles();
        p[0].profile_index = 4;
        assert_eq!(p[1].addon_profile(&p), Ok(4));
        p[0].is_primary = false;
        assert!(p[1].addon_profile(&p).is_err());
    }

    #[test]
    fn explicit_addon_sharing_overrides_the_legacy_field() {
        let profile: Profile = serde_json::from_str(
            r#"{"profile_index":2,"uses_primary_addons":false,"uses_primary_plugins":true}"#,
        )
        .unwrap();
        assert!(!profile.uses_primary_addons);
        let legacy: Profile =
            serde_json::from_str(r#"{"id":2,"uses_primary_plugins":true}"#).unwrap();
        assert!(legacy.uses_primary_addons);
        assert_eq!(legacy.profile_index, 2);
    }
}
