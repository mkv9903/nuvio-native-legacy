#!/bin/bash
# Capturas nativas de Ajustes com fixtures locais, sem conta nem rede.
# Precisa de janela GL; inspecao visual no host nao valida uma TV fisica.
#
#   NUVIO_AJUSTES_UX=1 bash tests/ajustes_shot.sh /tmp/nuvio-ajustes
#   NUVIO_RAIL=fixa bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-fixa
#
# Salva 12 cenas da UX (incluindo Trailers no enquadramento do mockup).
# Sem NUVIO_AJUSTES_UX, acrescenta uma captura para cada categoria.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/ajustes_shot.c -Isrc -o "${NUVIO_SHOT_BIN:-/tmp/nuvio-ajustes-shot}" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${NUVIO_SHOT_BIN:-/tmp/nuvio-ajustes-shot}" "$@"
