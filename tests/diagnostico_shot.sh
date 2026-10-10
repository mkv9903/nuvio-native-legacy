#!/bin/bash
# Capturas da tela de diagnostico em BMP (inicio, rodando, dois resultados e o
# cabecalho de Ajustes), sem rede e sem interacao.
#
#   bash tests/diagnostico_shot.sh /tmp/nuvio-diag
#   NUVIO_SHOT_PT=1 bash tests/diagnostico_shot.sh /tmp/nuvio-diag-pt
#
# Nao entra na suite (tools/test-all.sh pula *_shot.sh): precisa de janela GL
# e de olho humano. NUVIO_DADOS e temporario: nada toca nos dados de quem roda.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-diag-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/diagnostico.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/diagnostico_shot.c -Isrc -o /tmp/nuvio-diagnostico-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-diagnostico-shot "$@"
