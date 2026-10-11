#!/bin/bash
# Captura das linhas de auto-play (#202) em Ajustes (PNG, sem rede). Nao entra
# na suite: precisa de janela GL e de olho humano.
#   mkdir -p /tmp/nv-202 && NUVIO_SHOT_FONTE=3 bash tests/autoplay_ajustes_shot.sh /tmp/nv-202/aj
set -eu
cd "$(dirname "$0")/.."
export NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-autoplay-dados.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/autoplay_ajustes_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined -w
"$NUVIO_DADOS/shot" "$@"
