#!/bin/bash
# Resolucao/codec de fonte de canal ao vivo, lidos do nome. Ver tests/canal_res.c.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/canal_res.c -Isrc -o /tmp/nuvio-canal-res \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-canal-res; rm -f /tmp/nuvio-canal-res
