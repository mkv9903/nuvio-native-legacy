#!/bin/bash
# Capturas do painel de pausa. Nao entra na suite (*_shot.sh): janela GL.
#   bash tests/player_pausa_shot.sh /tmp/nv-player-live-shots/pausa
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/player_pausa_shot.c -Isrc -o /tmp/nuvio-player-pausa-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-player-pausa-shot "$@"
