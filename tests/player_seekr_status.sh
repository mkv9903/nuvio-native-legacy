#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/nuvio-player-seekr-status.XXXXXX)
trap 'rm -f "$binary"' EXIT
stripflag=-Wl,--gc-sections
cc tests/player_seekr_status.c src/seekr.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -O1 -g -ffunction-sections -fdata-sections "$stripflag" \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$binary"
"$binary"
