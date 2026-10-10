#!/bin/bash
# Icone essencial no nucleo TPK atualizado, sem janela GL nem TV.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-icone-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -ffunction-sections -fdata-sections -Wl,--gc-sections -Wno-deprecated-declarations \
  -include tests/gfx_icone_embutido_gl.h tests/gfx_icone_embutido.c src/gfx.c \
   -lSDL2 -lSDL2_image $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -o "$tmp/teste"
"$tmp/teste"
