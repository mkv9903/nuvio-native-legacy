#!/bin/bash
# Issue #132: a folha de Fontes lista tudo que os addons mandaram, nos dois
# modos de "Fonte automatica". Ver tests/fontes_lista.c.
#
#   bash tests/fontes_lista.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/fontes_lista.c -Isrc -o /tmp/nuvio-fontes-lista \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontes-lista
