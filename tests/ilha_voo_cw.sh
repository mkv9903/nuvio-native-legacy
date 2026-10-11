#!/bin/bash
# O voo do player ate a ilha com a refacao do "Continuar assistindo" no ar
# (ilha_voo_cw.c). Janela GL do Mac; nao entra na suite.
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-voo-cw.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/ilha_voo_cw.c -Isrc -o "$NUVIO_DADOS/test" \
  -O1 -g -DNV_LEVE -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$NUVIO_DADOS/test"
