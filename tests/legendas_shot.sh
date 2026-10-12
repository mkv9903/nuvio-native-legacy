#!/bin/bash
# Captures of the subtitle selector and dual subtitles (F04). Not in the suite
# (*_shot.sh): GL window. See tests/legendas_shot.c.
#   bash tests/legendas_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
bin="${TMPDIR:-/tmp}/nuvio-legendas-shot"
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/legendas_shot.c -Isrc -o "$bin" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D" "$bin"' EXIT
mkdir -p "$1"
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" "$bin" "$1"
