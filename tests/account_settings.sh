#!/usr/bin/env bash
# Retained C account-settings behavior; login/logout/profile tests live in Rust.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/test-support.sh
sources=()
for source in src/*.c; do [ "$source" = src/main.c ] || sources+=("$source"); done
directory=$(mktemp -d)
trap 'rm -rf "$directory"' EXIT
cc tests/conta_ajustes.c "${sources[@]}" -o "$directory/settings" -Isrc \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
  $(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib) -ldl -pthread -lm
"$directory/settings"
