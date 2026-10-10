#!/bin/bash
# Subtitle selector model (src/legendasui.c). See tests/legendasui.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legendasui.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} tests/legendasui.c src/linguas.c src/bidi.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -std=gnu11 -g -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
