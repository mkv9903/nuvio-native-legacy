#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
source tools/addon-test-support.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addon-subtitles.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/addons_legendas.c src/linguas.c src/js.c src/legextras.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -std=gnu11 -pthread -ffunction-sections -fdata-sections -Wl,--gc-sections -Wall -Wextra -Wno-unused-function -o "$work/test"
saida=$("$work/test")
if printf '%s\n' "$saida" | rg -q 'fixture\.invalid|PRIVATE_TOKEN|tt123|Movie|Show\.S01'; then
  echo "subtitle diagnostic leaked fixture URL, token, or content id" >&2
  exit 1
fi
printf '%s\n' "$saida" | rg -q 'resource=subtitles tipo=movie http=404'
printf '%s\n' "$saida" | rg -q 'array=1 recebidas=0 candidatas=0'
printf '%s\n' "$saida" | rg -q 'array=0 recebidas=0 candidatas=0'
printf '%s\n' "$saida"
cc "${flags[@]}" tests/addons_legendas_faixas.c -Isrc \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/menu"
"$work/menu"
