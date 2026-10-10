#!/bin/bash
# A home nos niveis de GPU 0, 1 e 2 (src/gpunivel.h), em BMP, com o
# preenchimento por modo e o tempo de GPU do Mac. Nao entra na suite
# (tools/test-all.sh pula *_shot.sh): precisa de janela GL, de REDE
# (metahub) e de olho humano. Ver tests/gpunivel_shot.c.
#
#   bash tests/gpunivel_shot.sh /tmp/nuvio-gpunivel
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-gpunivel-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

ENV_D=$(tools/env.sh --allow-unconfigured 2>/dev/null || true)
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
eval cc '"${sources[@]}"' tests/gpunivel_shot.c -Isrc -o /tmp/nuvio-gpunivel-shot \
  -O1 -g "$ENV_D" \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-gpunivel-shot "$NUVIO_DADOS" "$@"
