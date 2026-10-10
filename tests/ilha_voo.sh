#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ilha-voo.XXXXXX")
trap 'rm -f "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/ilha_voo.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$bin" -O1 -g ${flags[@]+"${flags[@]}"} -Wno-deprecated-declarations
"$bin"
