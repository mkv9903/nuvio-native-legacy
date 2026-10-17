#!/usr/bin/env bash
# Rust owns text handling; native UI tests cover the C projection boundary.
set -euo pipefail
cd "$(dirname "$0")/.."
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$PWD/build/backend/target}"
cargo test --manifest-path backend/Cargo.toml --locked --test search_text
