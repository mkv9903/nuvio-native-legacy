#!/bin/bash
# Capturas do painel "Carregamento da Home" da ilha. Nao entra na suite.
#   bash tests/homecarga_painel_shot.sh [prefixo]      (ANTES=<dir src antigo>: o painel antigo)
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=${TMPDIR:-/tmp/}
NUVIO_DADOS=$(mktemp -d "$TMPDIR/nuvio-hcp-dados.XXXXXX"); export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
S=${ANTES_SRC:-src}; extra=(); [ -n "${ANTES_SRC:-}" ] && extra+=(-DANTES)
sources=(); for s in $S/*.c; do [ "$s" != "$S/main.c" ] && sources+=("$s"); done
cc "${sources[@]}" tests/homecarga_painel_shot.c -I$S -o "$TMPDIR/homecarga-painel-shot" \
  -O1 -g -DNV_LEVE ${extra[@]+"${extra[@]}"} -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$TMPDIR/homecarga-painel-shot" "${1:-/tmp/nuvio-homecarga-painel}"
