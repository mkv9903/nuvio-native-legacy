use std::collections::HashMap;

/// Every application service property documented in local.properties.example.
/// Keep user logins and provider credentials out of this build configuration.
pub const SERVICE_PROPERTIES: &[(&str, &str)] = &[
    ("SUPABASE_URL", "NUVIO_SUPABASE_URL"),
    ("SUPABASE_ANON_KEY", "NUVIO_SUPABASE_ANON_KEY"),
    ("TV_LOGIN_BASE", "TV_LOGIN_WEB_BASE_URL"),
    ("TRAKT_CLIENT_ID", "TRAKT_CLIENT_ID"),
    ("TRAKT_CLIENT_SECRET", "TRAKT_CLIENT_SECRET"),
    ("SIMKL_CLIENT_ID", "SIMKL_CLIENT_ID"),
    ("SIMKL_APP_NAME", "SIMKL_APP_NAME"),
    ("TMDB_API_KEY", "TMDB_API_KEY"),
    ("SEEKR_API_KEY", "SEEKR_API_KEY"),
];

pub fn parse_properties(text: &str) -> HashMap<&str, &str> {
    text.lines()
        .map(str::trim)
        .filter(|line| !line.is_empty() && !line.starts_with(['#', '!']))
        .filter_map(|line| line.split_once('='))
        .map(|(name, value)| (name.trim(), value.trim()))
        .collect()
}

pub fn render_configuration(text: &str) -> String {
    let values = parse_properties(text);
    let mut config = String::from("// Application service configuration; no user credentials.\n");
    for (name, property) in SERVICE_PROPERTIES {
        let value = values.get(property).copied().unwrap_or("");
        // Rust's string escaping preserves tokens with '=', quotes and backslashes.
        config.push_str(&format!("pub const {name}: &str = {value:?};\n"));
    }
    config
}
