#!/bin/bash
# Canal ao vivo: ordem do zapping, debounce e escolha de agora/a seguir.
# Ver tests/aovivo.c.
#
#   bash tests/aovivo.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/aovivo.c src/epg.c tests/aovivo.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -o /tmp/nuvio-aovivo-tests -O1 -g -lz \
   -lSDL2 \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-aovivo-tests
