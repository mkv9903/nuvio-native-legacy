#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# #190: republica o catalogo com o player aberto. Liga o app inteiro menos o
# main, como tests/player.sh; nenhuma janela e aberta (nada aqui desenha).
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/player_republica.c -Isrc -o /tmp/nuvio-player-republica-tests \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-player-republica-tests "$@"
