#!/bin/bash
# Interface resolution: migration + 4K watch (src/resolucao.h). See the .c.
#
#   bash tests/resolucao.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/resolucao.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-resolucao-teste" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-resolucao-teste"
