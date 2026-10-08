#!/bin/bash
# Teste de regressao: invalidacao de fontes do alvo anterior
#   bash tests/streams_alvo_invalida.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done

if [ "$(uname)" = "Darwin" ]; then
  cc "${sources[@]}" tests/streams_alvo_invalida.c -Isrc -o /tmp/nuvio-streams-alvo-invalida \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
else
  DEPS=(sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)
  DEP_CFLAGS=($(pkg-config --cflags "${DEPS[@]}"))
  DEP_LIBS=($(pkg-config --libs "${DEPS[@]}"))
  cc "${sources[@]}" tests/streams_alvo_invalida.c -Isrc -o /tmp/nuvio-streams-alvo-invalida \
    -O1 -g "${DEP_CFLAGS[@]}" "${DEP_LIBS[@]}" -ldl -pthread -lm \
    -Wno-deprecated-declarations
fi

/tmp/nuvio-streams-alvo-invalida
