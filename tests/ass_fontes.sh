#!/bin/bash
# Queda da C9 no libass (ass_start_frame assert) ao reentregar fontes. Ver
# tests/ass_fontes.c. Precisa do libass do Homebrew; sem ele, pula.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_fontes: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_ASS_LIBASS -Isrc tests/ass_fontes.c src/assrender.c -o /tmp/nuvio-ass-fontes-test \
  $(pkg-config --cflags --libs libass) -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ass-fontes-test 2>/dev/null
