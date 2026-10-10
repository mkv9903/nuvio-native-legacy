#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
OUT="${TMPDIR:-/tmp}/nuvio-entrada-texto"
cc -O1 -Wall -Wextra -DNV_TEXTO_SDL_TESTE -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  tests/entrada_texto.c src/entrada_texto.c  -lSDL2 -o "$OUT"
"$OUT" | grep -v '^\[texto\]'
