#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,--gc-sections \
       -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
       -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# SO descoberta.c: as duas funcoes publicas testadas (desc_manifesto_cache_*)
# nao chamam rede, addons.c, SDL, ajustes, fileiras, colecoes, trakt nem
# nuvem — so a trava e a tabela estaticas do proprio arquivo. -dead_strip com
# secoes por funcao descarta o resto que este teste nao alcanca, entao nenhum
# desses modulos precisa de stub (mesma receita de tests/homepos.sh).
cc "${flags[@]}" src/descoberta.c src/cwordem.c tests/manifesto_cache.c \
  -o /tmp/nuvio-manifesto-cache-tests
/tmp/nuvio-manifesto-cache-tests
