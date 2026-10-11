#!/bin/bash
# A tecla de espaco da Busca e do Spotlight: digitar "the office" da "the
# office" (D-pad e teclado fisico) e o rotulo e a BARRA em todo idioma, nao o
# word for outer space.
#
#   bash tests/espaco.sh
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-espaco.XXXXXX")
export NUVIO_DADOS
bin="$NUVIO_DADOS.bin"
trap 'rm -rf "$NUVIO_DADOS" "$bin"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/espaco.c -Isrc -o "$bin" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
