#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Teste separado para preservar foco na sessao e reiniciar na primeira coluna.
#
# Mesma lista de tradutores do home.sh: home.c fala com catalogo, progresso,
# focus, ajustes, colecoes, catordem e fileiras.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/homepos.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-homepos-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-homepos-tests
