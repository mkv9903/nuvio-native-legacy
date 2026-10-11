#!/bin/bash
# Abas da pagina de colecao nos dois estados, em BMP. Nao entra na suite
# (tools/test-all.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/vertudo_shot.sh /tmp/nuvio-vertudo
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-vertudo-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
# src/vertudo.c e INCLUIDO pelo teste: desenharCard e a lista de canais sao
# estaticos, e semear por dentro e o unico jeito de fotografar sem addon no ar.
cc "${sources[@]}" tests/vertudo_shot.c -Isrc -o /tmp/nuvio-vertudo-shot \
  -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-vertudo-shot "$@"
