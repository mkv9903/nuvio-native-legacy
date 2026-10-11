#!/bin/bash
# #162: itens escondidos da barra lateral. Ver o .c.
#
#   bash tests/menu_ocultos.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/menu_ocultos.c -Isrc -o /tmp/nuvio-menu-ocultos \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-menu-ocultos
