#!/bin/bash
# Estrutura (colecoes/ordem/limite) mudando no meio da montagem nao descarta o
# que chegou; identidade (dono/perfil/addons) continua descartando.
#
#   bash tests/montagem_estrutura.sh
#
# montar() de verdade (descoberta.c por #include) com homeestado.c, catalogo.c,
# colecoes.c e catordem.c de verdade. Ver o cabecalho de tests/montagem_estrutura.c.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-montagem-estrutura.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT
export TMPDIR="$run_dir"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/montagem_estrutura.c src/cotacat.c src/homeestado.c src/catalogo.c \
  src/progress.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$run_dir/test" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$run_dir/test"
