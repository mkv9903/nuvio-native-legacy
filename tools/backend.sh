#!/usr/bin/env bash
# Build or test the Rust-owned backend without starting the application.
set -euo pipefail
cd "$(dirname "$0")/.."
task="${1:---build}"
export CARGO_TARGET_DIR="${CARGO_TARGET_DIR:-$PWD/build/backend/target}"
cd backend
case "$task" in
  --build) cargo build --locked ;;
  --release) cargo build --locked --release ;;
  --test) cargo test --locked ;;
  *) echo "Usage: bash tools/backend.sh [--build|--release|--test]" >&2; exit 2 ;;
esac
