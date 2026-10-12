use std::{collections::BTreeMap, env, fs, path::PathBuf};

const PROPERTIES: [&str; 9] = [
    "NUVIO_SUPABASE_URL",
    "NUVIO_SUPABASE_ANON_KEY",
    "TV_LOGIN_WEB_BASE_URL",
    "TRAKT_CLIENT_ID",
    "TRAKT_CLIENT_SECRET",
    "SIMKL_CLIENT_ID",
    "SIMKL_APP_NAME",
    "TMDB_API_KEY",
    "SEEKR_API_KEY",
];

fn main() {
    println!("cargo:rerun-if-env-changed=NUVIO_PROPERTIES");
    println!("cargo:rerun-if-changed=../local.properties");
    println!("cargo:rerun-if-changed=../local.properties.example");
    let property_path = env::var_os("NUVIO_PROPERTIES")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("../local.properties"));
    println!("cargo:rerun-if-changed={}", property_path.display());
    let properties: BTreeMap<_, _> = fs::read_to_string(property_path)
        .unwrap_or_default()
        .lines()
        .filter_map(|line| {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') {
                return None;
            }
            let (key, value) = line.split_once('=')?;
            Some((key.trim().to_owned(), value.trim().to_owned()))
        })
        .collect();
    let mut source =
        String::from("// Generated build configuration. Never log credential values.\n");
    source.push_str(&format!(
        "pub const CONFIGURATION_KEYS: &[&str] = &{PROPERTIES:?};\n"
    ));
    for name in PROPERTIES {
        println!("cargo:rerun-if-env-changed={name}");
        let value = env::var(name)
            .ok()
            .or_else(|| properties.get(name).cloned())
            .unwrap_or_default();
        source.push_str(&format!("pub const {name}: &str = {value:?};\n"));
    }
    let output = PathBuf::from(env::var_os("OUT_DIR").expect("Cargo output directory"));
    fs::write(output.join("service_config.rs"), source)
        .expect("write generated service configuration");
}
