#!/bin/bash
# Fila da ilha do relogio (prioridade, +N, central). Sem janela; ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc tests/ilhafila.c src/ilha.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -ffunction-sections -fdata-sections -Wl,--gc-sections -lSDL2 -lm -o /tmp/nuvio-ilhafila \
  -Wno-macro-redefined -Wno-deprecated-declarations
/tmp/nuvio-ilhafila
