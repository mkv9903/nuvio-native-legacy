#!/bin/bash
# "Tirar de Continuar assistindo" sai NA HORA e nao volta com o remoto velho.
#
#   bash tests/cwremover.sh
#
# Dois binarios, porque descoberta.c e home.c nao cabem no mesmo #include:
#   tests/cwremover.c      — fileira publicada, refacao com o "Trakt" falso
#                            (paused_at mais velho fica fora, mais novo volta),
#                            publicacao em voo, remontagem sem rede;
#   tests/cwremover_home.c tests/home_account_stub.c — a HOME remonta no mesmo quadro (guarda curto da
#                            1.4 em sincronizarFileiras).
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/cwordem.c src/cwretido.c tests/cwremover.c src/cotacat.c \
  src/js.c src/metaprov.c src/colecoes.c src/net_url.c src/catordem.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o /tmp/nuvio-cwremover-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
/tmp/nuvio-cwremover-tests
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/metaprov.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/cwremover_home.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-cwremover-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-cwremover-home-tests
