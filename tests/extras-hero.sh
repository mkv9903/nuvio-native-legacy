#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -DNUVIO_TRAILER_TEST -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -ffunction-sections -fdata-sections \
  src/extras.c src/js.c src/net.c src/net_url.c tests/extras-hero.c \
   -lSDL2 -lcurl -lpthread -Wl,--gc-sections \
  -o /tmp/nuvio-extras-hero-tests
/tmp/nuvio-extras-hero-tests
