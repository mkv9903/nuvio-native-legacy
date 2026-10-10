#!/bin/bash
# Janelas (ini, n) das fileiras publicadas apontando para itens de OUTRO bloco:
# "Amigos assistindo" com titulos do pacote e sem nome, medido na LG C9 com o
# trabalho do Codex. Ver tests/homejanelas.c.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/homejanelas.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c -o /tmp/nuvio-homejanelas-tests
/tmp/nuvio-homejanelas-tests
