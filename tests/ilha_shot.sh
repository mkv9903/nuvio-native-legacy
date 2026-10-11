#!/bin/bash
# Capturas da ilha do relogio (ilha.h) sobre arte, sem rede. Nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-ilha-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ] && [ "$source" != src/avisos.c ]; then
    sources+=("$source")
  fi
done
cc "${sources[@]}" tests/ilha_shot.c -Isrc -o /tmp/nuvio-ilha-shot \
  -O1 -g -DNV_LEVE -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ilha-shot "${1:-/tmp/nuvio-ilha}"
