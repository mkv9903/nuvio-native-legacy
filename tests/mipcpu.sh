#!/bin/bash
# Piramide de mipmaps na CPU (#201): tamanhos, cor media e acesso fora da
# superficie.
#
# SANITIZE=1 enables AddressSanitizer and UndefinedBehaviorSanitizer.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
out="${TMPDIR:-/tmp}/nuvio-mipcpu"
cc "${flags[@]}" "${sources[@]}" tests/mipcpu.c -Isrc -o "$out" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
