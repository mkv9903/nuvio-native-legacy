#!/bin/bash
# Arabe do TMDB/addon em todo caminho de texto (ver tests/arabe_texto.c).
# So fontes embarcadas, como na Samsung. Requer GL (Mac).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arabe-texto.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/arabe_texto.c -Isrc -o "$work/test" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_SEM_RESERVA_DE_SISTEMA=1 NUVIO_DADOS="$work" "$work/test" | grep -v "^\[t\]\|^fonte\|^reserva"
