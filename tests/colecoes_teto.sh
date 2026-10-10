#!/bin/bash
# #255: 7 colecoes, 296 pastas, 598 fontes e ordem de 830 itens na Home real.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -ffunction-sections -fdata-sections -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-colecoes-teto.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
cc "${flags[@]}" src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/colfileiras.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/colecoes_teto.c tests/home_account_stub.c -Wl,--gc-sections -o "$bin"
"$bin"
