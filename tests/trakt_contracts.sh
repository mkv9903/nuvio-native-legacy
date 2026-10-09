#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcontracts.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
stripflag=(-Wl,--gc-sections)
if [ "$(uname -s)" = Darwin ]; then stripflag=(-Wl,-dead_strip); fi
cc ${flags[@]+"${flags[@]}"} src/trakt.c tests/stub_fichameta.c tests/trakt_contracts.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections \
  "${stripflag[@]}" -lpthread
"$dir/teste"
