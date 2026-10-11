#!/bin/bash
# A atualizacao no Glass UI v2 (aviso na ilha, ilha crescendo, os estados do
# cartao, a barra na ilha, procurando/em dia), em BMP. Nao entra na suite
# (tools/test-all.sh pula *_shot.sh): precisa de janela GL e de olho humano.
#
#   bash tests/atualizacao_shot.sh /tmp/nuvio-upd                       # vidro
#   NUVIO_SHOT_VIDRO=0 bash tests/atualizacao_shot.sh /tmp/nuvio-upd-s  # solido
#
# NV_AT_INSTALA=1 encena a TV COM Homebrew Channel, que e o ramo com botoes e
# barra de progresso. Nenhum build de produto define esta macro.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-update-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/atualizacao.c) continue;; esac
  sources+=("$source")
done
# tools/env.sh injeta -DNV_VERSAO a partir do appinfo.json: sem ele a linha
# "Voce esta na X" sai como "dev", que nao e o que a TV mostra.
ENV_D=$(tools/env.sh)
eval cc "${sources[@]}" tests/atualizacao_shot.c -Isrc -o /tmp/nuvio-update-shot \
  -O1 -g -DNV_AT_INSTALA=1 "$ENV_D" \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-update-shot "$@"
