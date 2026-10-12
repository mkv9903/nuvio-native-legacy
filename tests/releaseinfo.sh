#!/bin/bash
# Release text regression test; all writes stay in a temporary directory.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-releaseinfo-XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/releaseinfo.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/releaseinfo.c -Isrc -o "$test_dir/test" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm
NUVIO_DADOS="$test_dir" "$test_dir/test"
