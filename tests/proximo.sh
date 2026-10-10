#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,--gc-sections -Isrc \
       -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
       -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# tests/proximo.c inclui src/proximo.c direto: a regra nao depende de nada do
# app (nem SDL, nem catalogo.c), entao nao ha o que linkar alem dela.
cc ${flags[@]+"${flags[@]}"} tests/proximo.c -o /tmp/nuvio-proximo-tests
/tmp/nuvio-proximo-tests
