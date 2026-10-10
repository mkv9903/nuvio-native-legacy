#!/bin/bash
# Parser sem inicializar SDL; os desenhos de badges sao descartados no link.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-stream-parser.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
flags+=(-Wl,--gc-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc src/stream_parse.c src/js.c src/badges.c tests/stream_parser.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) -O1 -g -Wall -Wextra -Wno-macro-redefined \
  "${flags[@]}" -o "$DIR/test"
"$DIR/test"
