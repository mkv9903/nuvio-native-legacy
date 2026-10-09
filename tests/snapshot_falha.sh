#!/bin/bash
# Catalogo que falhou ou veio vazio na volta que grava o snapshot da home
# volta a ser pedido na seguinte (#195).
#
#   bash tests/snapshot_falha.sh
#
# montar() de verdade (descoberta.c por #include) com homeestado.c, catalogo.c,
# colecoes.c e catordem.c de verdade. Ver o cabecalho de tests/snapshot_falha.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/snapshot_falha.c src/cotacat.c src/homeestado.c src/catalogo.c \
  src/progress.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o /tmp/nuvio-snapshot-falha-tests -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/tmp/nuvio-snapshot-falha-tests
