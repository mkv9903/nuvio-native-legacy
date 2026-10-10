#!/bin/bash
# Capturas da menu lateral e da folha de faixas (Interface de vidro), em BMP, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/vidro_menu_shot.sh /tmp/nuvio-vidro-menu
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-vidro-menu-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/vidro_menu_shot.c -Isrc -o /tmp/nuvio-vidro-menu-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-vidro-menu-shot "$@"
