#!/bin/bash
# Profile pill offset (tests/perfil_pilula.c), no GL. NUVIO_DADOS is temporary.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-perfil-pilula-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/perfil.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/perfil_pilula.c -Isrc -o "$tmp/teste" -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -w
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/teste"
