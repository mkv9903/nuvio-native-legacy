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
pub mod catalog_order;
pub mod catalog_order_ffi;
pub mod config;
pub mod content_context;
pub mod controller;
pub mod error;
pub mod ffi;
pub mod http;
mod json_store;
pub mod model;
mod platform;
pub mod service;
pub mod storage;
