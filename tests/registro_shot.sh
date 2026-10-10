#!/bin/bash
# Capturas do registro do app no Glass UI (tests/registro_shot.c) em BMP.
#
#   bash tests/registro_shot.sh <pasta> [ids...]
#   NUVIO_SHOT_VIDRO=0 bash tests/registro_shot.sh <pasta>     (material solido)
#
# Fundos: NUVIO_SHOT_ARTE_LOG (painel), NUVIO_SHOT_ARTE_AJ (Ajustes, aviso,
# queda, medidor), NUVIO_SHOT_ARTE_PLAYER (player); sem eles, a
# arte embarcada. Nao entra na suite (tools/test-all.sh pula *_shot.sh):
# precisa de janela GL e de olho humano. NUVIO_DADOS e temporario.
set -eu
cd "$(dirname "$0")/.."
[ $# -ge 1 ] || { echo "uso: $0 <pasta> [ids...]" >&2; exit 1; }
mkdir -p "$1"

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-registro-shot.XXXXXX)
export NUVIO_DADOS
B=$(mktemp ${TMPDIR:-/tmp}/nuvio-registro-shot-bin.XXXXXX)
trap 'rm -rf "$NUVIO_DADOS" "$B"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/log.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/registro_shot.c -Isrc -Itests -o "$B" \
  -DREGISTRO_TESTE -DAVISOS_TESTE -DNV_VERSAO='"1.7.2"' -DDESEMPENHO_TESTE -DAJUSTES_TESTE \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$B" "$@"
