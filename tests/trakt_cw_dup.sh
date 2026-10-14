#!/bin/bash
# #244 (cinco cards iguais, remocao parcial) e #243 ("14" inventado, nota ausente).
#   bash tests/trakt_cw_dup.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-traktcwdup.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc src/trakt.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_cw_dup.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -o "$dir/teste" -O1 -g -Wall -Wextra -ffunction-sections -fdata-sections -Wl,--gc-sections -lpthread
"$dir/teste" >/dev/null
COM_CERT=1 "$dir/teste" >/dev/null
SEM_NOTA=1 "$dir/teste" >/dev/null
"$dir/teste" | tail -1
