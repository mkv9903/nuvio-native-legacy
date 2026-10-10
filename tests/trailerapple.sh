#!/bin/bash
# Precisa de rede: fala com tv.apple.com.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  src/trailerapple.c src/net.c src/net_url.c src/js.c tests/trailerapple.c  -lSDL2 -lcurl -lpthread -o /tmp/nuvio-trailerapple-tests
/tmp/nuvio-trailerapple-tests
