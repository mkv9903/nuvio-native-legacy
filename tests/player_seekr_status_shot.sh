#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"
saida="${1:-/tmp/nuvio-player-seekr-status}"
mkdir -p "$saida"
taskDados=$(mktemp -d /tmp/nuvio-player-seekr-status-shot.XXXXXX)
trap 'rm -rf "$taskDados"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/player_seekr_status_shot.c -Isrc \
  -o "$taskDados/shot" -O1 -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
for vidro in 0 1; do
  NUVIO_SHOT_VIDRO=$vidro NUVIO_DADOS="$taskDados" NUVIO_TESTE_DIR="$taskDados" \
    "$taskDados/shot" "$saida"
done
for source in "$saida"/*.bmp; do
  "$IMAGE_TOOL" -background none "$source" "${source%.bmp}.png" >/dev/null
done
