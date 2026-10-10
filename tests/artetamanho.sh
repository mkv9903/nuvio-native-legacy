#!/bin/bash
# A variante do tamanho do desenho: a regra (artetamanho.c) e o cache
# (texvariante.c: chave, disco e promocao a heroi). Sem rede.
#
#   bash tests/artetamanho.sh
set -eu
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/artetamanho.c tests/artetamanho.c \
  -Isrc -o /tmp/nuvio-artetamanho-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations
/tmp/nuvio-artetamanho-tests

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
# Cada tamanho do TMDB com a largura real dele (a "original" em 1600).
"$IMAGE_TOOL" tests/amostra.jpg -resize "780x439!" /tmp/nuvio-texvariante-780.jpg >/dev/null
"$IMAGE_TOOL" tests/amostra.jpg -resize "1280x720!" /tmp/nuvio-texvariante-1280.jpg >/dev/null
"$IMAGE_TOOL" tests/amostra.jpg -resize "1600x900!" /tmp/nuvio-texvariante-1600.jpg >/dev/null
cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/texvariante.c -Isrc -o /tmp/nuvio-texvariante \
  -O1 -g -Wall -Wextra -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-texvariante /tmp/nuvio-texvariante-780.jpg \
  /tmp/nuvio-texvariante-1280.jpg /tmp/nuvio-texvariante-1600.jpg
