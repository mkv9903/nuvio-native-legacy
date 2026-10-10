#!/bin/bash
# Miniatura do Seekr no player, contra a API real. Fora da suite.
#   SEEKR_API_KEY=... bash tests/seekr_shot.sh /tmp/nv-seekr/shot
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/seekr_shot.c -Isrc -o /tmp/nuvio-seekr-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-seekr-shot "$@"
