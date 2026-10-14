#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcontracts.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
stripflag=(-Wl,--gc-sections)
cc ${flags[@]+"${flags[@]}"} src/trakt.c tests/stub_fichameta.c tests/trakt_contracts.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$dir/teste" -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections \
  "${stripflag[@]}" -lpthread
"$dir/teste"
