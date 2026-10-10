#!/bin/bash
# Ponteiro do Magic Remote (src/pointer.c, issue #99): hit-test, camadas,
# hover, clique = OK, rodinha, seta que esconde e os avisos 484/485.
#
#   bash tests/ponteiro.sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ponteiro.XXXXXX")
trap 'rm -f "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/ponteiro.c src/pointer.c -Isrc -DNV_LINUX_DESKTOP -o "$bin" -Wall -Wextra \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined -lm ${flags[@]+"${flags[@]}"}
"$bin"
