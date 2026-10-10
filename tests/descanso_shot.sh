#!/bin/bash
# Capturas da tela de descanso (vitrine e relogio). Precisa de janela GL; nao
# entra na suite. Ver tests/descanso_shot.c.
#   bash tests/descanso_shot.sh [prefixo]
set -eu
cd "$(dirname "$0")/.."
fontes=()
for f in src/*.c src/dts/*.c; do [ "$f" != src/main.c ] && fontes+=("$f"); done
cc "${fontes[@]}" tests/descanso_shot.c -Isrc -o /tmp/nuvio-descanso-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" NUVIO_SHOT_FONTE="${NUVIO_SHOT_FONTE:-3}" \
  /tmp/nuvio-descanso-shot "${1:-/tmp/nuvio-descanso}"
