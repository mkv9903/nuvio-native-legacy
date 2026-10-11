#!/bin/bash
# Home com cada combinacao de "Background do hero" x "Destaque com outra
# arte", em BMP. Nao entra na suite (tools/test-all.sh pula *_shot.sh):
# precisa de janela GL, de REDE (metahub, TMDB, Trakt) e de olho humano.
#
# As chaves do TMDB e do Trakt saem do local.properties do app web pelo
# tools/env.sh, como em tools/linux.sh — sem elas as fontes virtuais falham e a
# captura mostra a reserva (o que tambem e um caso valido de olhar).
#
#   bash tests/herologo_shot.sh /tmp/nuvio-herologo
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-herologo-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

ENV_D=$(tools/env.sh --allow-unconfigured 2>/dev/null || true)
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
eval cc '"${sources[@]}"' tests/herologo_shot.c -Isrc -o /tmp/nuvio-herologo-shot \
  -O1 -g "$ENV_D" \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-herologo-shot "$NUVIO_DADOS" "$@"
