#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwlocal.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/catalogo.c src/progress.c src/cwordem.c tests/cwlocal.c \
  src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$work/test" -O1 -g -Wall -Wno-deprecated-declarations \
  -Wno-macro-redefined -Wno-unused-function
"$work/test"
