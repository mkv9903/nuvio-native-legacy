#!/bin/bash
# Retorno de fim de fileira: a seta que bate na borda desloca e volta com
# mola, sem mexer no foco. Ver tests/bordafileira.c tests/home_account_stub.c.
#
# Sem sanitize por padrao pelo mesmo motivo de tests/fimfileira.sh (ASan
# trava no dyld antes do main). SANITIZE=1 continua aceito.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/bordafileira.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-bordafileira-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-bordafileira-tests "$@"
