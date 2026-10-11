#[path = "../build_support/service_config.rs"]
mod service_config;

use service_config::{parse_properties, render_configuration, SERVICE_PROPERTIES};
use std::collections::BTreeSet;

#[test]
fn every_documented_service_property_has_exactly_one_build_mapping() {
    let example = parse_properties(include_str!("../../local.properties.example"));
    let documented: BTreeSet<_> = example.keys().copied().collect();
    let mapped: BTreeSet<_> = SERVICE_PROPERTIES
        .iter()
        .map(|(_, property)| *property)
        .collect();
    assert_eq!(
        mapped, documented,
        "Service build configuration is incomplete"
    );
    assert_eq!(
        mapped.len(),
        SERVICE_PROPERTIES.len(),
        "Duplicate property mapping"
    );
    let constants: BTreeSet<_> = SERVICE_PROPERTIES.iter().map(|(name, _)| *name).collect();
    assert_eq!(
        constants.len(),
        SERVICE_PROPERTIES.len(),
        "Duplicate constant name"
    );
}

#[test]
fn all_service_values_reach_the_generated_rust_configuration() {
    let fixture = r#"
NUVIO_SUPABASE_URL=https://backend.example.invalid
NUVIO_SUPABASE_ANON_KEY=fixture-anon
TV_LOGIN_WEB_BASE_URL=https://login.example.invalid/tv
TRAKT_CLIENT_ID=fixture-trakt-id
TRAKT_CLIENT_SECRET=fixture-trakt-secret
SIMKL_CLIENT_ID=fixture-simkl-id
SIMKL_APP_NAME=Fixture Rust App
TMDB_API_KEY=fixture-tmdb-key
SEEKR_API_KEY=fixture-seekr-key
"#;
    let expected = [
        ("SUPABASE_URL", "https://backend.example.invalid"),
        ("SUPABASE_ANON_KEY", "fixture-anon"),
        ("TV_LOGIN_BASE", "https://login.example.invalid/tv"),
        ("TRAKT_CLIENT_ID", "fixture-trakt-id"),
        ("TRAKT_CLIENT_SECRET", "fixture-trakt-secret"),
        ("SIMKL_CLIENT_ID", "fixture-simkl-id"),
        ("SIMKL_APP_NAME", "Fixture Rust App"),
        ("TMDB_API_KEY", "fixture-tmdb-key"),
        ("SEEKR_API_KEY", "fixture-seekr-key"),
    ];
    let generated = render_configuration(fixture);
    for (name, value) in expected {
        assert!(generated.contains(&format!("pub const {name}: &str = {value:?};")));
    }
}

#[test]
fn property_comments_and_string_escaping_do_not_change_credentials() {
    let text = "# TMDB_API_KEY=comment\r\n! another comment\r\n TMDB_API_KEY = fixture=a=b\r\nSIMKL_APP_NAME=Test \"TV\" \\ app\n";
    let values = parse_properties(text);
    assert_eq!(values["TMDB_API_KEY"], "fixture=a=b");
    assert!(!values.contains_key("# TMDB_API_KEY"));
    let generated = render_configuration(text);
    assert!(generated.contains("pub const TMDB_API_KEY: &str = \"fixture=a=b\";"));
    assert!(generated.contains("pub const SIMKL_APP_NAME: &str = \"Test \\\"TV\\\" \\\\ app\";"));
    assert!(generated.contains("pub const TRAKT_CLIENT_ID: &str = \"\";"));
}
