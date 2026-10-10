#!/bin/bash
# Cartao de canal do Guia nos dois estados, em BMP. Nao entra na suite
# (tools/test-all.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/guia_shot.sh /tmp/nuvio-guia
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-guia-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c) continue;; esac
  sources+=("$source")
done
# src/guia.c e INCLUIDO pelo teste: desenharCard e a lista de canais sao
# estaticos, e semear por dentro e o unico jeito de fotografar sem addon no ar.
cc "${sources[@]}" tests/guia_shot.c -Isrc -o /tmp/nuvio-guia-shot \
  -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-guia-shot "$@"
