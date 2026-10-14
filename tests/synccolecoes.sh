#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-misleading-indentation)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-synccolecoes.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-synccolecoes-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/colecoes.c src/colfileiras.c src/fileiras.c src/net_url.c src/catalog_order_adapter.c src/contacache.c src/js.c src/jsw.c tests/synccolecoes.c -o "$bin"
for case in network copy fallback isolated no-copy; do
  export NV_T_DIR="$dir/$case/space.mkv.nuvio.rusttest"; mkdir -p "$NV_T_DIR"
  "$bin" "$case"
done
