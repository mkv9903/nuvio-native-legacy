//! Rust-owned application capabilities consumed by the existing C UI.
//! Platform adapters do not determine the shape of the domain models.

pub const TEST_APP_ID: &str = "space.mkv.nuvio.rusttest";
pub mod account;
pub mod config;
pub mod error;
pub mod http;
pub mod model;
pub mod service;
pub mod storage;
