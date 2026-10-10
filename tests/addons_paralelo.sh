#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addons-par.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/addons_paralelo.c src/linguas.c src/js.c src/legextras.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -std=gnu11 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
