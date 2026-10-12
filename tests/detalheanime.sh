#!/bin/bash
# Detalhe de anime: tipo incerto nao vira filme (tests/detalheanime.c).
#
#   bash tests/detalheanime.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# detalheanime.c inclui descoberta.c inteiro (buscarEps e deMeta sao static),
# com o mesmo conjunto de link de tests/cateps.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/detalheanime.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-detalheanime-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-detalheanime-tests
