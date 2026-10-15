#!/bin/bash
# Ajustes por perfil nesta TV. Ver o .c.
#
#   bash tests/ajustes_perfil.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ajustes_perfil.c -Isrc -o /tmp/nuvio-ajustes-perfil \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ajustes-perfil
