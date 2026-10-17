#!/usr/bin/env bash
# Rust recent-query behavior and guarded per-profile storage.
set -euo pipefail
cd "$(dirname "$0")/.."
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$PWD/build/backend/target}"
cargo test --locked --manifest-path backend/Cargo.toml --test search_history --test search_history_runtime
