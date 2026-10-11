#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
output="${TMPDIR:-/tmp}/nuvio-sistexto-webos"
cc -O1 -Wall -Wextra -DNV_WEBOS -DNV_TEXTO_SDL_TESTE \
  $(pkg-config --cflags sdl2) tests/sistexto_webos.c src/sistexto.c src/entrada_texto.c \
  -lSDL2 -o "$output"
"$output"
