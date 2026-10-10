#!/bin/bash
# Renderer de familias tipograficas e legenda SRT. Janela GL oculta.
#   bash tests/text_familias.sh /tmp/nuvio-fontes
set -eu
cd "$(dirname "$0")/.."
prefix=${1:-/tmp/nuvio-fontes}
mkdir -p "$(dirname "$prefix")"
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/text_familias.c -Isrc -o /tmp/nuvio-text-familias \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/minimo/fonts" "$tmp/dados"
cp deploy/app/fonts/InterDisplay-{Regular,Medium,Bold}.ttf "$tmp/minimo/fonts/"
NUVIO_DADOS="$tmp/dados" /tmp/nuvio-text-familias \
  "$prefix" "$tmp/minimo"
