#!/bin/bash
# Abrir titulo sem espera (R2) (tests/abrirtitulo.c).
#
#   bash tests/abrirtitulo.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# abrirtitulo.c inclui descoberta.c inteiro (buscarEps e deMeta sao static),
# com o mesmo conjunto de link de tests/cateps.sh.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/cwordem.c tests/abrirtitulo.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nv-r2/abrirtitulo-tests -O1 -g -DCOM_SEMENTE \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nv-r2/abrirtitulo-tests
