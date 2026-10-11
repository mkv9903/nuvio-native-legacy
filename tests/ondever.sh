#!/bin/bash
# Onde assistir: servico do TMDB casa com o app da TV. Ver tests/ondever.c.
#
#   bash tests/ondever.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/ondever.c -Isrc -o "$work/test" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
