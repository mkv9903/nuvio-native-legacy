#!/bin/bash
# Capturas do overlay ASS. Nao entra na suite (*_shot.sh): janela GL.
#   bash tests/legenda_ass_shot.sh /tmp/nuvio-legass
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
ass_flags=()
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libass; then
  # O mesmo binario de captura exercita o caminho completo quando libass esta
  # instalado. Em builders sem a dependencia, o parser legado continua sendo
  # coberto e o diagnostico deixa claro que o backend nao entrou.
  ass_flags+=( -DNV_ASS_LIBASS )
  # shellcheck disable=SC2207
  ass_flags+=( $(pkg-config --cflags libass) )
  ass_libs=( $(pkg-config --libs libass) )
else
  ass_libs=()
fi
cc "${sources[@]}" tests/legenda_ass_shot.c -Isrc "${ass_flags[@]}" -o /tmp/nuvio-legass-shot \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  "${ass_libs[@]}" \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" /tmp/nuvio-legass-shot "$@"
