#!/bin/bash
# Reserva de arte pelo TMDB. Sem rede: o teste substitui rede_baixar e a chave.
#
#   bash tests/artereserva.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artereserva.c src/artefontes.c src/js.c src/net_url.c tests/artereserva.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-artereserva-tests -O1 -g -Wno-macro-redefined \
  -Wall -Wextra -Wno-deprecated-declarations
/tmp/nuvio-artereserva-tests
