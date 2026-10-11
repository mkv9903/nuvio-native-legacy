#!/bin/bash
# Capturas dos tres paineis de audiencia, em BMP, sem rede e sem interacao.
# Nao entra na suite (o testa-tudo.sh pula *_shot.sh): precisa de janela GL e de
# olho humano para julgar.
#
#   bash tests/serieaud_shot.sh /tmp/nuvio-serieaud
#
# Compila tudo MENOS src/serieaud.c, que a captura inclui para poder carregar os
# numeros medidos direto nos vetores do modulo — sem rede, sem fio e sem cache.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/serieaud.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/serieaud_shot.c -Isrc -o /tmp/nuvio-serieaud-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-serieaud-shot "$@"
