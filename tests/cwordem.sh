#!/bin/bash
# A Ordenacao de "Continuar assistindo" faz efeito (issue #127).
#
#   bash tests/cwordem.sh
#
# Tres binarios, pelo mesmo motivo de tests/cwremover.sh (descoberta.c e home.c
# nao cabem no mesmo #include):
#   tests/cwordem.c       — a regra pura (src/cwordem.c): ordem por modo,
#                           estreias, conjunto de futuros;
#   tests/cwordem_desc.c  — montarContinuar com um "Trakt" falso: a ordem que
#                           sai em cada modo e o showUnairedNextUp;
#   tests/cwordem_home.c tests/home_account_stub.c  — a HOME parte a retomada e poe "Proximos episodios"
#                           logo abaixo dela so em "Separar futuros".
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
out=/tmp
cc ${flags[@]+"${flags[@]}"} src/cwordem.c src/cwretido.c tests/cwordem.c -Isrc \
  -o "$out/nuvio-cwordem-tests" -O1 -g -Wall -Wextra
"$out/nuvio-cwordem-tests"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/cwordem.c src/cwretido.c tests/cwordem_desc.c \
  src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catalog_order_adapter.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$out/nuvio-cwordem-desc-tests" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$out/nuvio-cwordem-desc-tests"
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/posterprov.c src/net_url.c src/colecoes.c \
  src/js.c src/metaprov.c src/catalog_order_adapter.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c src/fonteregra.c tests/cwordem_home.c tests/home_account_stub.c \
  -Isrc -o "$out/nuvio-cwordem-home-tests" -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$out/nuvio-cwordem-home-tests"
echo "cwordem: tudo ok"
