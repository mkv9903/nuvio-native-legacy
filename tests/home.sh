#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Linka somente a lógica exercitada, sem inicializar SDL/AppKit nem vídeo.
#
# src/fileiras.c ENTRA NA LISTA porque home_layout.c chama sincronizarFileiras,
# que passou a consultar fil_unir/fil_limite quando a escolha local de fileiras
# nasceu. A lista e escrita a mao, entao ela nao acompanha sozinha: o teste
# ficou sem linkar ("_fil_unir referenced from _sincronizarFileiras") e o
# script morria antes de rodar um caso sequer.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progress.c src/focus.c src/ajustes.c src/fonteregra.c src/posterprov.c src/net_url.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c src/cwretido.c tests/home_layout.c tests/home_account_stub.c \
  -Isrc -o /tmp/nuvio-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-home-tests
