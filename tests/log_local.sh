#!/bin/bash
# Local log logic only: no window or network.
set -eu
cd "$(dirname "$0")/.."
B=$(mktemp "${TMPDIR:-/tmp}/nuvio-log-local-bin.XXXXXX")
trap 'rm -f "$B"' EXIT
cc tests/log_local.c src/data.c src/idioma.c -Isrc -DNV_LINUX_DESKTOP \
  -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  $(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf glesv2) -lm -o "$B"
"$B"
