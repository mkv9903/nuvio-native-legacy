#!/bin/bash
# Um titulo, um lugar: o que o cartao da ilha / a faixa "Retomar agora" segura
# fica fora de "Continuar assistindo" e volta na frente quando sai (cwretido.h).
#
#   bash tests/cwretido.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
out="${TMPDIR:-/tmp}/nuvio-cwretido-home-tests"
cc src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catalog_order_adapter.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c src/fonteregra.c tests/cwretido_home.c tests/home_account_stub.c \
  -Isrc -o "$out" -O1 -g -ffunction-sections -fdata-sections \
  -DNV_SHOT_HOOKS -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
