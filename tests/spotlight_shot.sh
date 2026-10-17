#!/usr/bin/env bash
# Exercise retained C search navigation and appearance with the real Rust history.
set -eu
cd "$(dirname "$0")/.."
export CARGO_TARGET_DIR="$PWD/build/backend/ui-target"
cargo build --locked --manifest-path backend/Cargo.toml --features native-ui-tests
export NUVIO_TEST_BACKEND_LIB="$CARGO_TARGET_DIR/debug/libnuvio_backend.a"
source tools/test-support.sh
IMAGE_TOOL="$(command -v magick || command -v convert)"
saida="${1:-/tmp/nuvio-spotlight}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spot-shot-XXXXXX")"
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spot-dados-XXXXXX")"
# addon de canais falso (o mesmo de spotlight_canais.sh) para a captura -canal
python3 tests/spotlight_canais_servidor.py 8767 2>/dev/null & SRV=$!
trap 'kill $SRV 2>/dev/null || true; rm -rf "$tmp" "$dados"' EXIT
sources=()
for source in src/*.c; do
  [ "$source" = "src/main.c" ] && continue
  sources+=("$source")
done
cc "${sources[@]}" tests/spotlight_shot.c -Isrc -o "$tmp/shot" -O1 -g \
  -DNV_LINUX_DESKTOP -DNV_NATIVE_UI_TEST '-DNV_APP_ID="space.mkv.nuvio.rusttest"' $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$dados" SPOT_CANAIS_BASE="http://127.0.0.1:8767" "$tmp/shot" "$saida"
if command -v "$IMAGE_TOOL" >/dev/null 2>&1; then
  for b in "$saida"-*.bmp; do
    "$IMAGE_TOOL" "$b" -resize "1400x1400>" "${b%.bmp}.jpg" >/dev/null
  done
fi
