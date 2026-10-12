#!/bin/bash
# Concurrent discovery starts must create one catalogue worker.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/test-support.sh
tmp="${TMPDIR:-/tmp}"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread); fi
cc ${flags[@]+"${flags[@]}"} -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/desc_iniciar_corrida.c src/homeestado.c src/catalogo.c \
  src/progress.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c src/cotacat.c \
  -Isrc -Itests -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
  -o "$tmp/nuvio-desc-iniciar-corrida" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$tmp/nuvio-desc-iniciar-corrida"
