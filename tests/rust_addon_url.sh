#!/bin/bash
# Real Rust staticlib against the legacy C oracle, then the existing C service
# integration scenarios with each implementation. No provider/network access.
set -euo pipefail
cd "$(dirname "$0")/.."
addon_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-addonurl.XXXXXXXX")
trap 'rm -rf "$addon_scratch"' EXIT
rust_library=$(bash tools/rust-core.sh)
flags=(-O1 -g -Isrc -DNV_RUST_CORE=1 -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/addonurl_reference.c tests/rust_addon_url.c "$rust_library" \
  -ldl -lpthread -lm -lrt -o "$addon_scratch/parity"
"$addon_scratch/parity" > "$addon_scratch/output"
cat "$addon_scratch/output"
if ! grep -qF '[addons] Addon: URL length 2048 exceeds maximum 2047: add-on skipped' "$addon_scratch/output"; then
  echo 'FAIL: English URL rejection missing' >&2; exit 1
fi
if ! grep -qF '[addons] Sample: request length 32 exceeds maximum 31: skipped' "$addon_scratch/output"; then
  echo 'FAIL: English request rejection missing' >&2; exit 1
fi
if ! grep -qF '[addons] Sample: request length -1 exceeds maximum 0: skipped' "$addon_scratch/output"; then
  echo 'FAIL: zero-capacity request diagnostic' >&2; exit 1
fi
if grep -qE 'https?://|stremio://|key=|hidden|SECRET' "$addon_scratch/output"; then
  echo 'FAIL: provider URL/credential in diagnostic output' >&2; exit 1
fi
bash tests/addonurl.sh
bash tests/addonurl.sh --rust
