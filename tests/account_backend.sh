#!/usr/bin/env bash
# Account, session, storage, HTTP and profile tests are implemented in Rust.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tools/backend.sh --test
