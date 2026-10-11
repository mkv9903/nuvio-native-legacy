#!/bin/bash
# Captura de Ajustes > Integracoes > "Notas no titulo" (PNG, sem rede). Nao
# entra na suite: precisa de janela GL e de olho humano.
#   mkdir -p /tmp/nv-notas-shots && bash tests/notas_ajustes_shot.sh /tmp/nv-notas-shots/aj
set -eu
cd "$(dirname "$0")/.."
export NUVIO_DADOS=$(mktemp -d /tmp/nuvio-notasaj-dados.XXXXXX)
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/notas_ajustes_shot.c -Isrc -o /tmp/nuvio-notasaj-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined -w
/tmp/nuvio-notasaj-shot "$@"
