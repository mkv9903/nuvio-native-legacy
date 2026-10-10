#!/bin/bash
# O hero desenha a arte do titulo em foco, nao a do anterior (issue #118).
# Ver tests/heroidentidade_home.c tests/home_account_stub.c.
set -eu
cd "$(dirname "$0")/.."
cc src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/heroidentidade_home.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-heroidentidade -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-heroidentidade
