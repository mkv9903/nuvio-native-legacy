#!/bin/bash
# Capturas dos estados novos da ilha (mockup de 02/10), em PNG da faixa de
# cima. NUVIO_SHOT_FUNDOS=<pasta com bgNN.png> poe o fundo do quadro do
# mockup atras. Nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-ilha3-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/ilha3_shot.c -Isrc -o /tmp/nuvio-ilha3-shot \
  -O1 -g -DNV_LEVE -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ilha3-shot "${1:-/tmp/nuvio-ilha3}"
