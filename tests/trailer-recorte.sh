#!/bin/bash
# trailer_recorte: o "cover" da fonte do trailer no destino (trailer.h).
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/trailer-recorte.c -Isrc -o "$tmp/t" -O0 \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined 2>"$tmp/build.log" || { cat "$tmp/build.log" >&2; exit 1; }
"$tmp/t"
