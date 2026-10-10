#!/bin/bash
# Live channel Subtitles sheet: no addon (movie) subtitles. See tests/faixas_canal.c.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-faixas-canal.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/faixas_canal.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
