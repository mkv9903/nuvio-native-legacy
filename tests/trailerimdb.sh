#!/bin/bash
# Precisa de rede: fala com api.graphql.imdb.com.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  src/trailerimdb.c src/net.c src/net_url.c src/js.c tests/trailerimdb.c  -lSDL2 -lcurl -lpthread -o /tmp/nuvio-trailerimdb-tests
/tmp/nuvio-trailerimdb-tests "${1:-tt2012616}"
