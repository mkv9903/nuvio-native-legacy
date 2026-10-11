#!/bin/bash
# Captura da folha de fontes com a marca "Sua escolha anterior" (issue #56).
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/fontepref_shot.sh /tmp/nuvio-fontepref
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-fontepref-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/fontepref_shot.c -Isrc -o /tmp/nuvio-fontepref-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontepref-shot "$@"
