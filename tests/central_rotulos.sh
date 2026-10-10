#!/bin/bash
# Os nomes dos 22 botoes da Central cabem no botao, nos 30 idiomas, em
# Montserrat. Ver tests/central_rotulos.c (janela GL escondida).
#
#   bash tests/central_rotulos.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-central-rotulos.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/central_rotulos.c -Isrc -o "$work/test" -DNV_SHOT_HOOKS \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DADOS="$work/dados" "$work/test"
