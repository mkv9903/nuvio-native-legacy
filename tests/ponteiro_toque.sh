#!/bin/bash
# #216: regressao de toque puro, sem inicializador Cocoa/SDL no host.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ponteiro-toque.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Wall -Wextra -Isrc -DNV_LINUX_DESKTOP -lm -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) )
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/ponteiro_toque.c src/pointer.c -o "$dir/teste" -lm
"$dir/teste"
