#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/atualizacao.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/atualizacao_agenda.c -Isrc -o /tmp/nuvio-atualizacao-agenda \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-atualizacao-agenda "$@"
