#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pausa-lg.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/video_pausa_lg.c -o "$dir/test" \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm
"$dir/test"
