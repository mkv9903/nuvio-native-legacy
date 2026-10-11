#!/bin/bash
# A extensao de informacoes do menu do cartaz na Home (ctxinfo_shot.c), em PNG,
# com a fonte da TV por padrao (NUVIO_SHOT_FONTE=3). Janela GL escondida do
# Mac; sem rede.
#
#   bash tests/ctxinfo_shot.sh [pasta]
set -eu
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nv-ctxinfo-shots}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ctxinfo-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/ctxinfo_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "ctxinfo|ctxmenu|error" "$NUVIO_DADOS/build.log" || true
NUVIO_SHOT_FONTE="${NUVIO_SHOT_FONTE:-3}" "$NUVIO_DADOS/shot" "$saida"
