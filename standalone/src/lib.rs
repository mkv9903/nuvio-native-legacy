//! Standalone Rust application. No dependency on the legacy application or core.
pub mod account;
pub mod catalog;
pub mod error;
pub mod http;
pub mod platform;
pub mod profile;
pub mod secret;
pub mod storage;

pub mod service_config {
    include!(concat!(env!("OUT_DIR"), "/service_config.rs"));
}

pub const APP_ID: &str = "space.mkv.nuvio.rusttest";
pub const APP_NAME: &str = "Nuvio Rust Test";
pub const APP_VERSION: &str = env!("CARGO_PKG_VERSION");
