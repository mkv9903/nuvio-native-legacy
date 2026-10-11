#!/bin/bash
# Fundo pelo caminho da luz imersiva (Frost / Arte borrada): o quadro pequeno recebe a pintura e a tela
# assada bate com o desenho direto. Ver tests/fundo_assado.c.
#
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fundo-assado.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
gl=($(pkg-config --libs glesv2 egl) -ldl -pthread -lm)
cc "${sources[@]}" tests/fundo_assado.c -Isrc -o "$work/test" \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz "${gl[@]}" \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$work/dados"
NUVIO_DUMP_FUNDO_DIR="$work" NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/test" 2>&1 |
  grep -vE "^\[(tex|arte|cat|desc|home|txt|corviva|ajustes|dados|perfil|badges)[a-z-]*\]"
exit "${PIPESTATUS[0]}"
