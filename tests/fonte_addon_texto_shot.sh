#!/bin/bash
# Sources sheet in add-on text mode (Texto das fontes = Do addon), fed by real
# Stremio JSON. Not in the suite (GL window).
#   NUVIO_SHOT_FONTE=3 bash tests/fonte_addon_texto_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-fonte-addon-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/fonte_addon_texto_shot.c -Isrc -o "$bin" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "$1"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "$1/addon"
