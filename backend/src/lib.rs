//! Rust-owned application capabilities consumed by the existing C UI.
//! Platform adapters do not determine the shape of the domain models.

pub const TEST_APP_ID: &str = "space.mkv.nuvio.rusttest";
pub mod account;
pub mod addon_backend;
pub mod addon_controller;
pub mod addon_edits;
pub mod addon_ffi;
pub mod addon_service;
pub mod addon_snapshot;
pub mod addon_storage;
pub mod addons;
pub mod catalog_cache;
pub mod catalog_ffi;
pub mod catalog_order;
pub mod catalog_order_ffi;
pub mod catalog_runtime;
pub mod catalog_scene;
pub mod catalog_titles;
pub mod config;
pub mod content_context;
pub mod controller;
pub mod error;
pub mod ffi;
pub mod home_loading;
pub mod home_loading_ffi;
pub mod home_order_backend;
pub mod home_order_controller;
pub mod http;
mod json_store;
pub mod model;
#[cfg(feature = "native-ui-tests")]
mod native_addon_fixture;
mod platform;
pub mod provider_http;
pub mod provider_loader;
pub mod scene_backend;
pub mod scene_cache;
pub mod scene_capture;
pub mod scene_snapshot;
pub mod service;
pub mod storage;
pub mod title_snapshot;
