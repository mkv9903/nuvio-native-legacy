#!/bin/bash
# Capturas de salvar na ilha (ilhasalvar.h): voo da capa, avisos e a pergunta
# de primeira vez, em PNG. Nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-ilhasalvar-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/ilhasalvar_shot.c -Isrc -o /tmp/nuvio-ilhasalvar-shot \
  -O1 -g -DNV_LEVE -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ilhasalvar-shot "${1:-/tmp/nuvio-ilhasalvar}"
