#!/bin/bash
# #244: ver tests/cwdup244.c
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/cwordem.c src/cwretido.c tests/cwdup244.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$TMPDIR/nuvio-cwdup244" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$TMPDIR/nuvio-cwdup244"
