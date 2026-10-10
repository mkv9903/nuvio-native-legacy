#!/bin/bash
# ASS na area do video (4:3, 2.39:1, escala). Ver tests/ass_area.c.
# Precisa do libass do Homebrew; sem ele, pula.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_area: libass ausente (teste ignorado)"; exit 0
fi
cc -DNV_ASS_LIBASS -include tests/ass_pisca_gl.h -Isrc tests/ass_area.c src/assrender.c \
  -o /tmp/nuvio-ass-area-test \
  $(pkg-config --cflags --libs libass) -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-ass-area-test 2>/dev/null
