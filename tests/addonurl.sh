#!/usr/bin/env bash
# URL domain and native ownership cases were migrated to Rust.
set -euo pipefail
cd "$(dirname "$0")/.."
cargo test --locked --manifest-path backend/Cargo.toml --test addons --test addon_snapshot
