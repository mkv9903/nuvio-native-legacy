#!/bin/bash
# Captura da folha de fontes com emoji/bandeira/versalete de addon (issue #144).
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/fontes_glifos_shot.sh /tmp/nuvio-fontes-glifos
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-fontes-glifos-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/fontes_glifos_shot.c -Isrc -o /tmp/nuvio-fontes-glifos-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontes-glifos-shot "$@"
