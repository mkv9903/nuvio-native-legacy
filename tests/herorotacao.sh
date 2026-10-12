#!/bin/bash
# Rotacao automatica do destaque: o que a segura e o que a desliga (voltar um
# titulo, trailer tocando). Ver tests/herorotacao.c tests/home_account_stub.c.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
cc src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catalog_order_adapter.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/herorotacao.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-herorotacao-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-herorotacao-tests "$@"
