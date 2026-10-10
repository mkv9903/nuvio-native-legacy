#!/bin/bash
# Exercita o scheduler real de src/app.c com resolver Stalker bloqueavel.
set -eu
cd "$(dirname "$0")/.."
cc src/cwordem.c src/cwretido.c tests/stalker_scheduler.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-stalker-scheduler -O0 -g -Wall -Wextra \
  -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
  -Wl,--gc-sections \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -pthread
timeout 5 /tmp/nuvio-stalker-scheduler
