#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever-sheet.XXXXXX")
NUVIO_DADOS="$work/data";mkdir -p "$NUVIO_DADOS";export NUVIO_DADOS
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c;do
  case "$source" in src/main.c|src/ondever.c) continue;;esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ondever_folha.c -Isrc -o "$work/test" -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
