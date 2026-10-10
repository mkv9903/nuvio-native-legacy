#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legauto.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/legauto.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -std=gnu11 -g -Wall -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
