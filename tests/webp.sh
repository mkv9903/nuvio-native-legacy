#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
san=()
if [ "${SANITIZE:-0}" = 1 ]; then san=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -O1 -g -Wall -Wextra -pthread "${san[@]}" -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   -Wno-deprecated-declarations \
  src/webp.c tests/webp.c -lSDL2 -o /tmp/nuvio-webp-tests
/tmp/nuvio-webp-tests
