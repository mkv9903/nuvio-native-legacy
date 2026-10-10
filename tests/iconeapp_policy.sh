#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/nuvio-icon-policy.XXXXXX)
trap 'rm -f "$binary"' EXIT
cc src/iconeapp.c tests/iconeapp_policy.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -o "$binary"
"$binary"
