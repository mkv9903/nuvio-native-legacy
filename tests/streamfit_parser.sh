#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
SF_DIR=$(mktemp -d /tmp/nuvio-streamfit-parser.XXXXXX)
trap 'rm -rf "$SF_DIR"' EXIT
flags=(-O1 -g -Wall -Wextra -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) -ffunction-sections -fdata-sections)
flags+=(-Wl,--gc-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc "${flags[@]}" tests/streamfit_parser.c src/stream_parse.c src/js.c src/badges.c -o "$SF_DIR/test"
"$SF_DIR/test"
