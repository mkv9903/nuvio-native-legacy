#!/bin/bash
# Home e pagina de titulo com "Da arte", "Gradiente", "Imersiva" e "Textura", em BMP,
# sem rede (artes de deploy/app/art). Nao entra na suite (testa-tudo.sh pula
# *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/corviva_shot.sh /tmp/nuvio-corviva
#   convert /tmp/nuvio-corviva-1-home-dinamica-vermelho.bmp x.png
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-corviva-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/corviva_shot.c -Isrc -o /tmp/nuvio-corviva-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined 2>&1 \
  | grep -E "corviva_shot|error" || true
/tmp/nuvio-corviva-shot "$@"
