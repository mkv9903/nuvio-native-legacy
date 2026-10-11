#!/bin/bash
# Painel de Salvos/Social aberto por varios segundos: a home de tras (fundo
# parado, spainel_fundo) nao pode sumir. Precisa de GL; escreve so em
# NUVIO_DADOS, uma pasta temporaria.  bash tests/spainel_fundo_tempo.sh
# A medida de tempo por quadro e outra: tests/salvospainel_perf.sh.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spft-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/spainel_fundo_tempo.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/teste" | grep -vE "^\[(tex|arte|cat|desc|home|salvos|txt|jpeg|t|extras|tex-trace)\]|^(reserva|fonte|catalogo: [0-9]+ (ids|episodios|titulos)|home: )"
