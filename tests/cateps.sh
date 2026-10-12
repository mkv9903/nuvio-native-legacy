#!/bin/bash
# Faixas de episodio contra o catalogo que cresce.
#
#   bash tests/cateps.sh
#
# So src/catalogo.c, como tests/catfileira.sh: a regra e sobre indices e nao
# depende de SDL, de rede nem da descoberta.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# cateps.c inclui descoberta.c inteiro (pelo desc_tmdb_notas_temporada, pura),
# entao o link precisa do mesmo conjunto de tests/colfileiras.sh mais o
# catalogo.c de verdade — que e o objeto do teste original.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/cateps.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c \
  -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-cateps-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-cateps-tests
