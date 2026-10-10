#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cwfrente.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/cwfrente.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$work/test" -O1 -g -Wall -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
