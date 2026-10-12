#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
out="${TMPDIR:-/tmp}/nuvio-proximo-desfoque-tests"
cc src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catalog_order_adapter.c src/fileiras.c src/artehero.c src/cwordem.c tests/proximo_desfoque_home.c tests/home_account_stub.c \
  -Isrc -o "$out" -O1 -g -ffunction-sections -fdata-sections \
  -DNV_SHOT_HOOKS -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
