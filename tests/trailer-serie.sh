#!/bin/bash
# #123: trailers de serie no detalhe (ficha TMDB com videos en/null, pt-BR).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -DNUVIO_TRAILER_TEST -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -ffunction-sections -fdata-sections \
  src/extras.c src/js.c src/net.c src/net_url.c tests/trailer-serie.c \
   -lSDL2 -ldl -lpthread -Wl,--gc-sections \
  -o /tmp/nuvio-trailer-serie-tests
/tmp/nuvio-trailer-serie-tests
