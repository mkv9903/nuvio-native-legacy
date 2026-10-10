#!/bin/bash
# Capturas da folha de episodios e do menu de visto, em BMP, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-antes
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-episodios-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/episodios_shot.c -Isrc -o /tmp/nuvio-episodios-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-episodios-shot "$@"
