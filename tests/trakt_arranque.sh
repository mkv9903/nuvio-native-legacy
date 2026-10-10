#!/bin/bash
# B2: tempo de trakt_continuar/trakt_social contra rede falsa com latencia.
#   bash tests/trakt_arranque.sh
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktarr.XXXXXX")
trap 'rm -rf "$dir"' EXIT
stripflag=(-Wl,--gc-sections)
cc src/trakt.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_arranque.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$dir/teste" -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections "${stripflag[@]}" -lpthread
"$dir/teste" | grep -E 'arranque-trakt|PASS'
