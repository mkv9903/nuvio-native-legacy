use nuvio_backend::{
    addon_edits::{AddonChange, AddonEdit},
    addons::{AddonUrl, InstalledAddon},
};

fn addon(id: &str, enabled: bool) -> InstalledAddon {
    InstalledAddon {
        name: id.into(),
        url: AddonUrl::parse(&format!("https://{id}.invalid/config")).unwrap(),
        enabled,
    }
}

#[test]
fn offline_toggle_preserves_phone_additions_names_order_and_other_changes() {
    let a = addon("a", true);
    let edit = AddonEdit::new(AddonChange::SetEnabled {
        url: a.url.clone(),
        enabled: false,
    });
    let mut renamed = a;
    renamed.name = "Phone name".into();
    let latest = vec![addon("c", true), renamed, addon("b", false)];
    let merged = edit.merge(&latest);
    assert_eq!(
        merged
            .iter()
            .map(|item| item.name.as_str())
            .collect::<Vec<_>>(),
        ["c", "Phone name", "b"]
    );
    assert!(!merged[1].enabled);
    assert!(!merged[2].enabled);
    assert_eq!(edit.merge(&merged), merged); // Retrying a push is idempotent.
}

#[test]
fn phone_removal_wins_over_a_toggle_but_explicit_tv_install_reinstalls() {
    let a = addon("a", true);
    let mut edit = AddonEdit::new(AddonChange::SetEnabled {
        url: a.url.clone(),
        enabled: false,
    });
    let latest = vec![addon("b", true), addon("c", true)];
    assert_eq!(edit.merge(&latest), latest);
    edit.append(AddonChange::Install { addon: a.clone() });
    let merged = edit.merge(&latest);
    assert_eq!(merged[2], a);
    assert_eq!(edit.merge(&merged), merged);
}

#[test]
fn pending_tv_value_wins_conflicts_and_installs_do_not_duplicate_providers() {
    let a = addon("a", true);
    let mut edit = AddonEdit::new(AddonChange::Install { addon: a.clone() });
    edit.append(AddonChange::SetEnabled {
        url: a.url.clone(),
        enabled: false,
    });
    let merged = edit.merge(&[a.clone()]);
    assert_eq!(merged.len(), 1);
    assert!(!merged[0].enabled);
    assert_eq!(edit.merge(&merged), merged);
    assert_eq!(edit.merge(&[]), merged);
}

#[test]
fn removes_only_target_provider_and_remove_then_install_preserves_action_order() {
    let a = addon("a", true);
    let b = addon("b", true);
    let mut edit = AddonEdit::new(AddonChange::Remove { url: a.url.clone() });
    assert_eq!(edit.merge(&[a.clone(), b.clone()]), [b.clone()]);
    edit.append(AddonChange::Install { addon: a.clone() });
    assert_eq!(edit.merge(&[a.clone(), b.clone()]), [b, a]);
}

#[test]
fn journals_round_trip_full_unicode_long_urls_and_reject_unknown_actions() {
    let url = AddonUrl::parse(&format!(
        "https://addon.invalid/{}?secret=keep/",
        "config".repeat(1000)
    ))
    .unwrap();
    let mut edit = AddonEdit::new(AddonChange::Install {
        addon: InstalledAddon {
            name: "Quotes \" and 日".into(),
            url: url.clone(),
            enabled: true,
        },
    });
    let before = edit.id;
    edit.append(AddonChange::SetEnabled {
        url: url.clone(),
        enabled: false,
    });
    edit.append(AddonChange::SetEnabled { url, enabled: true });
    assert_ne!(before, edit.id);
    assert_eq!(edit.changes.len(), 2);
    let bytes = serde_json::to_vec(&edit).unwrap();
    assert_eq!(serde_json::from_slice::<AddonEdit>(&bytes).unwrap(), edit);
    assert!(!format!("{edit:?}").contains("secret=keep"));
    let mut value = serde_json::to_value(&edit).unwrap();
    value["changes"][0]["action"] = "future_unsupported_action".into();
    assert!(serde_json::from_value::<AddonEdit>(value).is_err());
}
