#!/bin/bash
# Capturas do OSD do canal ao vivo, do banner do zapping e do cartao de erro.
# Nao entra na suite (*_shot.sh): janela GL e olho humano.
#   bash tests/aovivo_shot.sh /tmp/nv-player-live-shots/aovivo
# NUVIO_SHOT_VIDRO=1 liga a interface de vidro; NUVIO_SHOT_4K=1 desenha num FBO 3840x2160.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/aovivo_shot.c -Isrc -o /tmp/nuvio-aovivo-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-aovivo-shot "$@"
