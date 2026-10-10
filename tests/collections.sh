#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/collections_data.c src/js.c src/metaprov.c -o /tmp/nuvio-collections-data-tests
/tmp/nuvio-collections-data-tests
cc ${flags[@]+"${flags[@]}"} tests/badges.c -o /tmp/nuvio-badges-tests
/tmp/nuvio-badges-tests
