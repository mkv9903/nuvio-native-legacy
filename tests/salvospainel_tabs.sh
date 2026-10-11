#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spainel-tabs-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/salvospainel.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/salvospainel_tabs.c -Isrc -o "$test_dir/test" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm
NUVIO_DADOS="$test_dir" "$test_dir/test"
