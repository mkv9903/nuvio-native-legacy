#!/bin/bash
# "Retomar agora" sem relogio (retomar_shot.c), em PNG. Janela GL do Mac; nao
# entra na suite.
set -eu
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"

saida="${1:-/tmp/nv-retomar-shots}"
mkdir -p "$saida"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-retomar-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
# Nunca executar um binario velho se o compilador falhar. O filtro anterior
# terminava em `|| true` e escondia inclusive erros de link.
if ! cc "${sources[@]}" tests/retomar_shot.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g ${NV_CFLAGS:-} -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
grep -E "retomar_shot|error|home\.c" "$NUVIO_DADOS/build.log" || true
"$NUVIO_DADOS/shot" "$saida/${NV_PREFIXO:-r}"
[ -n "${NV_KEEP:-}" ] && exit 0   # deixa os BMP, para comparar byte a byte
for f in "$saida"/*.bmp; do
  [ -e "$f" ] || continue
  "$IMAGE_TOOL" -background none "$f" "${f%.bmp}.png" >/dev/null 2>&1 && rm -f "$f"
done
