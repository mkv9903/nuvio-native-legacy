#!/bin/bash
# Quem liga o alerta "Carregamento da Home" da ilha. Ver tests/homecarga_ilha.c.
#   bash tests/homecarga_ilha.sh        (ANTES=1: so os casos que o codigo antigo ja tinha)
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
extra=()
if [ "${ANTES:-0}" = 1 ]; then extra+=(-DANTES); fi
cc ${extra[@]+"${extra[@]}"} -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/homecarga_ilha.c src/homeestado.c src/catalogo.c \
  src/progress.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c src/cotacat.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-homecarga-ilha-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/tmp/nuvio-homecarga-ilha-tests
