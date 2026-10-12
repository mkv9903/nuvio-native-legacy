//! Account/profile identity carried by all private content work.
use crate::model::ProfileId;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct AddonScope {
    pub member: String,
    pub owner: String,
    pub profile: ProfileId,
}
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ContentContext {
    pub account_generation: u64,
    pub content_generation: u64,
    pub profile: ProfileId,
    pub addons: AddonScope,
    pub force_primary: bool,
}

/// Private Home/cache ownership uses the selected profile, even when its
/// installed-addon list is inherited from the primary profile.
#[derive(Clone, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct ContentScope {
    pub member: String,
    pub owner: String,
    pub profile: ProfileId,
}
impl From<&ContentContext> for ContentScope {
    fn from(context: &ContentContext) -> Self {
        Self {
            member: context.addons.member.clone(),
            owner: context.addons.owner.clone(),
            profile: context.profile,
        }
    }
}
