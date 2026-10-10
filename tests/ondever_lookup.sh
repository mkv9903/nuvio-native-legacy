#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever-lookup.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/ondever_lookup.c src/js.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -std=gnu11 -pthread -Wall -Wextra -Wno-unused-function ${NV_CFLAGS:-} -o "$work/test"
"$work/test"
