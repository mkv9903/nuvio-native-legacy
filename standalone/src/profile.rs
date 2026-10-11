//! Profile identity is attached to every asynchronous Home load.
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Deserialize, Serialize, PartialEq, Eq)]
pub struct Profile {
    pub profile_index: u32,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub is_primary: bool,
    #[serde(default, alias = "uses_primary_plugins")]
    pub uses_primary_addons: bool,
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
}
