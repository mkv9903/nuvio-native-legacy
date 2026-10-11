#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
rust_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-core.XXXXXXXX")
trap 'rm -rf "$rust_scratch"' EXIT
rust_library=$(bash tools/rust-core.sh)
cc -O1 -g -Isrc tests/rust_core_probe.c "$rust_library" -ldl -lpthread -lm -lrt -o "$rust_scratch/probe"
"$rust_scratch/probe" "$rust_scratch"
if [ -n "$(find "$rust_scratch" -name '.rust-core-probe-*' -print -quit)" ]; then
  echo 'FAIL: Rust probe left a temporary file' >&2; exit 1
fi
