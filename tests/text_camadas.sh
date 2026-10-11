#!/bin/bash
# Two zoomed text layers in one frame keep their lines (no per-frame re-raster).
#   bash tests/text_camadas.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-text-camadas"
cc "${sources[@]}" tests/text_camadas.c -Isrc -o "$bin" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS=$(mktemp -d) "$bin"
