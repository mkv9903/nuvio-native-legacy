#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=c11 -D_GNU_SOURCE -O1 -g -ffunction-sections -fdata-sections -Isrc \
  tests/video_acb_lg.c src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c src/webosver.c src/mkv.c -Wl,--gc-sections -o "$tmp/test" "${sdl_flags[@]}" -ldl -pthread -lm
"$tmp/test"
