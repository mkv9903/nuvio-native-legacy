#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/colfileiras.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c -o /tmp/nuvio-colfileiras-tests
/tmp/nuvio-colfileiras-tests
cc ${flags[@]+"${flags[@]}"} tests/colcusto.c src/js.c src/colecoes.c src/net_url.c -o /tmp/nuvio-colcusto
/tmp/nuvio-colcusto
