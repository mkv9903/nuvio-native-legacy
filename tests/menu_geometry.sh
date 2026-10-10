#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc tests/menu_geometry.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -lSDL2 -lm -o /tmp/nuvio-menu-geometry
/tmp/nuvio-menu-geometry
