#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/nuvio-diag-persistencia.XXXXXX)
trap 'rm -f "$binary"' EXIT
# Dead-strip uncalled UI/network paths; no SDL runtime or whole-core build.
stripflag=-Wl,--gc-sections
cc tests/diagnostico_persistencia.c src/perfiltv.c -Isrc -DNV_LINUX_DESKTOP \
  -O1 -g -pthread -ffunction-sections -fdata-sections "$stripflag" \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$binary"
"$binary"
