#!/bin/bash
# Fim de fileira (issue #65): o foco chega a ultima coluna e continua recebendo
# seta, com republicacao concorrente do catalogo. Ver tests/fimfileira.c tests/home_account_stub.c.
#
# Sem janela nem inicializacao SDL: o relogio e um duble deterministico do
# teste. SANITIZE=1 verifica a logica sem carregar SDL/AppKit no macOS.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catalog_order_adapter.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/fimfileira.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-fimfileira-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fimfileira-tests "$@"
