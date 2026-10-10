#!/bin/bash
# Motor dos plugins Nuvio (src/pluginjs.c) — F09. Ver tests/pluginjs.c.
#
#   bash tests/pluginjs.sh                 # normal
#   SANITIZE=1 bash tests/pluginjs.sh      # ASan + UBSan
#   SANITIZE=thread bash tests/pluginjs.sh # TSan
set -eu
cd "$(dirname "$0")/.."
out="${NV_TMP:-/tmp/nv-f09-tmp}"; mkdir -p "$out"
tmp=$(mktemp -d "$out/pluginjs.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/plugins_server.py "$tmp/porta" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/porta" ] && break; sleep 0.1; done
base="http://127.0.0.1:$(cat "$tmp/porta")"
flags=(-O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -pthread -Wno-macro-redefined -ffunction-sections -fdata-sections)
flags+=(-Wl,--gc-sections)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer) ;;
esac
flags+=(-ldl)
cc "${flags[@]}" src/qjs.c src/pluginjs.c src/plugrede.c src/htmlq.c src/net.c src/net_url.c \
  src/stream_parse.c src/badges.c src/js.c tests/pluginjs.c -o "$tmp/t" 2>&1 | grep -E "error|undefined" && exit 1
"$tmp/t" "$base" | tee "$tmp/log"
grep -q 'pluginjs: ok' "$tmp/log"
echo "pluginjs.sh: ok"
