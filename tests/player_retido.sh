#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/test-support.sh
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-retido.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -Dsessao_usuario=nv_test_usuario -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_retido.c "$dir/player.o" "$dir/video.o" \
  -o "$dir/test"  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm
if [ "${NV_LLDB:-0}" = 1 ]; then
  lldb --batch -o run -k 'bt all' -- "$dir/test"
else
  "$dir/test"
fi
