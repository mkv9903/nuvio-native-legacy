#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/home_progressiva.c src/homeestado.c src/catalogo.c \
  src/progress.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c src/cotacat.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-home-progressiva-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
for mode in 0 1 2 3 4; do /tmp/nuvio-home-progressiva-tests "$mode"; done
