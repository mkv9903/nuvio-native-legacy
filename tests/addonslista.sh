#!/usr/bin/env bash
# Addon backend assertions now live in Rust; retained source logic stays in C.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/addon-test-support.sh
cargo test --locked --manifest-path backend/Cargo.toml --test addons --test addon_snapshot --test addon_backend
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,--gc-sections
       -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf))
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/addons.c src/js.c tests/source_status.c -o /tmp/nuvio-source-status-tests
/tmp/nuvio-source-status-tests
