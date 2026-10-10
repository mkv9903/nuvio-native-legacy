#!/bin/bash
# Teste de regressao: invalidacao de fontes do alvo anterior
#   bash tests/streams_alvo_invalida.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done

  DEPS=(sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)
  DEP_CFLAGS=($(pkg-config --cflags "${DEPS[@]}"))
  DEP_LIBS=($(pkg-config --libs "${DEPS[@]}"))
  cc "${sources[@]}" tests/streams_alvo_invalida.c -Isrc -o /tmp/nuvio-streams-alvo-invalida \
    -O1 -g -DNV_LINUX_DESKTOP "${DEP_CFLAGS[@]}" "${DEP_LIBS[@]}" -ldl -pthread -lm \
    -Wno-deprecated-declarations

/tmp/nuvio-streams-alvo-invalida
