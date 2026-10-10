#!/bin/bash
# Politica de arte de TELA CHEIA. Sem SDL, sem rede e sem disco: artehero.c so
# olha a url que o catalogo guarda e devolve outra string.
#
#   bash tests/artehero.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artehero.c tests/artehero.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-artehero-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-artehero-tests
