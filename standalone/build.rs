#[path = "build_support/service_config.rs"]
mod service_config;

use std::{env, fs, path::PathBuf};

fn main() {
    let properties = PathBuf::from(
        env::var_os("NUVIO_PROPERTIES").unwrap_or_else(|| "../local.properties".into()),
    );
    println!("cargo:rerun-if-env-changed=NUVIO_PROPERTIES");
    println!("cargo:rerun-if-changed={}", properties.display());
    let text = match fs::read_to_string(properties) {
        Ok(text) => text,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => String::new(),
        Err(_) => panic!("Application service configuration could not be read"),
    };
    let config = service_config::render_configuration(&text);
    let output = PathBuf::from(env::var_os("OUT_DIR").unwrap()).join("service_config.rs");
    fs::write(output, config).expect("write build configuration");
}
