#!/bin/bash
# Barra de progresso animada e onda de grade (tests/revelaprog.c).
#
#   bash tests/revelaprog.sh
set -eu
cd "$(dirname "$0")/.."
cc tests/revelaprog.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lm -o /tmp/nuvio-revelaprog-tests -O1 -g -Wall -Wextra
/tmp/nuvio-revelaprog-tests
