#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-streamfit-diagnostic.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/streamfit_diagnostico_server.py "$tmp/port" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep .1; done
test -s "$tmp/port"
flags=(-std=gnu11 -O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -pthread -ffunction-sections -fdata-sections -Wno-deprecated-declarations -Wno-macro-redefined)
flags+=(-Wl,--gc-sections -ldl)
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${NV_TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/streamfit_diagnostico.c src/streamfitdiag.c src/net_epoch.c src/streamfit.c src/vazao.c src/net.c src/net_url.c -o "$tmp/test"
"$tmp/test" "$(cat "$tmp/port")"
