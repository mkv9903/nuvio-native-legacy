#!/bin/bash
# Capturas dos paineis de frases e de ficha, em BMP, sem rede e sem interacao.
# Nao entra na suite (o testa-tudo.sh pula *_shot.sh): precisa de janela GL e de
# olho humano para julgar.
#
#   bash tests/seriefrases_shot.sh /tmp/nuvio-seriefrases
#
# Compila tudo MENOS src/seriefrases.c, que a captura inclui para poder carregar os
# numeros medidos direto nos vetores do modulo — sem rede, sem fio e sem cache.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/seriefrases.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/seriefrases_shot.c -Isrc -o /tmp/nuvio-seriefrases-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-seriefrases-shot "$@"
