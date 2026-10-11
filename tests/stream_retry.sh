#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/stream_retry.c -Isrc -o /tmp/nuvio-stream-retry \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-stream-retry
