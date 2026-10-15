#!/bin/bash
# Busca local e dados de comparacao, sem abrir janela ou fazer requisicoes.
#   bash tests/ajustes_ux_dados.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
binary="$(mktemp /tmp/nuvio-ajustes-ux-dados.XXXXXX)"
trap 'rm -f "$binary"' EXIT
cc "${sources[@]}" tests/ajustes_ux_dados.c -Isrc -o "$binary" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary"
