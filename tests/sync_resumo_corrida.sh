#!/bin/bash
# Stress the real summary publication/reader without network or graphics.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sync-summary.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=()
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
cc "${flags[@]}" -O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP \
  $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
  -ffunction-sections -fdata-sections -Wl,--gc-sections \
  tests/sync_resumo_corrida.c -o "$tmp/test"
"$tmp/test"
