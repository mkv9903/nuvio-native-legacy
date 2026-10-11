#!/bin/bash
# Long configured URLs survive install/export/reload/request construction.
# Oversized URLs are rejected, and diagnostics never expose their contents.
# The same integration cases exercise either implementation without a network.
#   bash tests/addonurl.sh [--rust]
set -euo pipefail
cd "$(dirname "$0")/.."
addon_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addonurl.XXXXXXXX")
trap 'rm -rf "$addon_scratch"' EXIT
read -r -a dependency_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections '-Wl,--gc-sections' \
       -DNV_LINUX_DESKTOP "${dependency_flags[@]}" \
       -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
libraries=()
# The C oracle retains its original diagnostics; only Rust Test translates them.
refusal='[addons] Large: URL de 2100 caracteres nao cabe (maximo 2047): addon ignorado'
implementation=C
case "${1:-}" in
  '') ;;
  --rust)
    flags+=(-DNV_RUST_CORE=1)
    libraries+=("$(bash tools/rust-core.sh)" -ldl -lpthread -lm -lrt)
    refusal='[addons] Large: URL length 2100 exceeds maximum 2047: add-on skipped'
    implementation=Rust ;;
  *) echo 'Usage: bash tests/addonurl.sh [--rust]' >&2; exit 2 ;;
esac
if [ "$#" -gt 1 ]; then echo 'Usage: bash tests/addonurl.sh [--rust]' >&2; exit 2; fi
# Discard unused sections; the test supplies the neighboring service doubles.
cc "${flags[@]}" src/addons.c src/js.c tests/addonurl.c "${libraries[@]}" -o "$addon_scratch/test"
output=$("$addon_scratch/test")
printf '%s\n' "$output" | grep -v '^\[addons\]' || true
printf '%s\n' "$output" | grep -q 'addonurl: ok'
# One rejection from loading the file and one from installing the add-on.
count=$(printf '%s\n' "$output" | grep -cF "$refusal" || true)
[ "$count" = 2 ] || { echo "FAIL: expected 2 rejection messages, got $count"; exit 1; }
if printf '%s\n' "$output" | grep -q 'SECRET'; then echo 'FAIL: configured URL appeared in the log'; exit 1; fi
echo "addonurl.sh: $implementation ok"
