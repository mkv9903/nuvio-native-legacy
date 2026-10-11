#!/bin/bash
# #210: rodape da rail recolhida sem nome do perfil. Ver o .c.
#
#   bash tests/rail_rodape_recolhida.sh [prefixo-dos-bmp]
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/rail_rodape_recolhida.c -Isrc -o /tmp/nuvio-rail-rodape \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-rail-rodape "$@"
