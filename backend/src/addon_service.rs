//! Authenticated account-addon operations with context-bound requests.
use crate::{
    account::AccountBackend,
    addons::{InstalledAddon, ADDON_RESPONSE_LIMIT},
    content_context::ContentContext,
    error::{Error, Result},
    http::Response,
};
use serde::Serialize;
use zeroize::Zeroizing;

pub trait AddonAccount: Send + Sync {
    fn current_context(&self, force_primary: bool) -> Option<ContentContext>;
    fn is_current(&self, context: &ContentContext) -> bool;
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>>;
    fn write_addons(&self, context: &ContentContext, addons: &[InstalledAddon]) -> Result<()>;
}
impl AddonAccount for AccountBackend {
    fn current_context(&self, force_primary: bool) -> Option<ContentContext> {
        self.content_context(force_primary)
    }
    fn is_current(&self, context: &ContentContext) -> bool {
        self.content_context_current(context)
    }
    fn read_addons(&self, context: &ContentContext) -> Result<Vec<InstalledAddon>> {
        const PAGE_SIZE: usize = 500;
        let mut result = Vec::new();
        let mut bytes = 0;
        loop {
            let query = url::form_urlencoded::Serializer::new(String::new())
                .append_pair("user_id", &format!("eq.{}", context.addons.owner))
                .append_pair(
                    "profile_id",
                    &format!("eq.{}", context.addons.profile.get()),
                )
                .append_pair("select", "*")
                .append_pair("order", "sort_order.asc")
                .append_pair("limit", &PAGE_SIZE.to_string())
                .append_pair("offset", &result.len().to_string())
                .finish();
            let response: Response = self.content_table(context, "addons", &query)?;
            response.success()?;
            bytes += response.body.len();
            if bytes > ADDON_RESPONSE_LIMIT {
                return Err(Error::ResponseLimit);
            }
            let page = InstalledAddon::decode_account_list(&response.body)?;
            let count = page.len();
            result.extend(page);
            if count < PAGE_SIZE {
                return Ok(result);
            }
        }
    }
    fn write_addons(&self, context: &ContentContext, addons: &[InstalledAddon]) -> Result<()> {
        #[derive(Serialize)]
        struct Item<'a> {
            url: &'a crate::addons::AddonUrl,
            sort_order: usize,
            enabled: bool,
            #[serde(skip_serializing_if = "str::is_empty")]
            name: &'a str,
        }
        #[derive(Serialize)]
        struct Push<'a> {
            p_profile_id: u32,
            p_addons: Vec<Item<'a>>,
        }
        let request = Push {
            p_profile_id: context.addons.profile.get(),
            p_addons: addons
                .iter()
                .enumerate()
                .map(|(sort_order, addon)| Item {
                    url: &addon.url,
                    sort_order,
                    enabled: addon.enabled,
                    name: &addon.name,
                })
                .collect(),
        };
        let bytes = Zeroizing::new(serde_json::to_vec(&request).map_err(|_| Error::InvalidData)?);
        if bytes.len() > ADDON_RESPONSE_LIMIT {
            return Err(Error::ResponseLimit);
        }
        self.content_rpc(context, "sync_push_addons", &bytes)?
            .success()
    }
}
