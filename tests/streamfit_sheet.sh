#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
SF_DIR=$(mktemp -d /tmp/nuvio-streamfit-sheet.XXXXXX)
trap 'rm -rf "$SF_DIR"' EXIT
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
       -Wall -Wextra -ffunction-sections -fdata-sections)
flags+=(-Wl,--gc-sections)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc "${flags[@]}" tests/streamfit_sheet.c src/streamfit.c src/vazao.c src/badges.c src/fonteauto.c src/naovideo.c -lm -o "$SF_DIR/test"
"$SF_DIR/test"
