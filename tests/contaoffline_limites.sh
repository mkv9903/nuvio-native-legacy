#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
cc "${flags[@]}" tests/contaoffline_limites.c src/catalog_order_adapter.c \
  src/contacache.c src/js.c src/jsw.c -o "$dir/contaoffline_limites"
NV_T_DIR="$dir" "$dir/contaoffline_limites"
