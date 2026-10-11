#!/bin/bash
# Native Linux UI preview. Video playback is stubbed for desktop UI testing.
#   bash tools/linux.sh           # build and run
#   bash tools/linux.sh --build   # build only
#   bash tools/linux.sh --preview # browse UI without an account
#   bash tools/linux.sh --rust-test --preview # isolated C/Rust test app
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_ONLY=0
RUST_TEST=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BUILD_ONLY=1; shift ;;
    --preview) export NUVIO_UI_PREVIEW=1; shift ;;
    --rust-test) RUST_TEST=1; shift ;;
    *) break ;;
  esac
done
CC="${CC:-cc}"
DEPS=(sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)
if ! pkg-config --exists "${DEPS[@]}"; then
  echo "linux.sh: install SDL2, SDL2_image, SDL2_ttf, GLES2, EGL and zlib development packages (see README.md)." >&2
  exit 2
fi

ENVF=$(mktemp)
PREVIEW_STAGE=''
cleanup() {
  rm -f "$ENVF"
  if [ -n "$PREVIEW_STAGE" ]; then rm -rf -- "$PREVIEW_STAGE"; fi
}
trap cleanup EXIT
bash tools/env.sh --env-file "$ENVF"
DEFINES=(-DNV_LINUX_DESKTOP)
OUTPUT=build/linux/nuvio-native
ART_ROOT="$PWD/deploy/app/art"
RUST_LIBS=()
if [ "$RUST_TEST" = 1 ]; then
  OUTPUT=build/linux-rust/nuvio-rust-test
  DEFINES+=(-DNV_RUST_CORE=1)
  RUST_LIBS+=("$(bash tools/rust-core.sh)" -lrt)
fi
while IFS='=' read -r name value; do
  # Pass each definition as one argument, including literal C string quotes.
  value=${value//\\/\\\\}
  value=${value//\"/\\\"}
  DEFINES+=("-D$name=\"$value\"")
done < "$ENVF"
read -r -a DEP_CFLAGS <<< "$(pkg-config --cflags "${DEPS[@]}")"
read -r -a DEP_LIBS <<< "$(pkg-config --libs "${DEPS[@]}")"
mkdir -p "$(dirname "$OUTPUT")"
"$CC" src/*.c -Isrc -o "$OUTPUT" -O1 -g \
  "${DEFINES[@]}" "${DEP_CFLAGS[@]}" "${RUST_LIBS[@]}" "${DEP_LIBS[@]}" -ldl -pthread -lm
echo "Linux UI build: $PWD/$OUTPUT"
if [ "$RUST_TEST" = 1 ]; then
  PREVIEW_STAGE=$(mktemp -d "$PWD/build/linux-rust/preview-stage.XXXXXXXX")
  bash tools/stage-rust-preview.sh deploy/app "$OUTPUT" "$PREVIEW_STAGE/app"
  rm -rf build/linux-rust/preview-app
  mv "$PREVIEW_STAGE/app" build/linux-rust/preview-app
  ART_ROOT="$PWD/build/linux-rust/preview-app/art"
  echo "Isolated preview resources: $ART_ROOT"
fi
if [ "$BUILD_ONLY" = 1 ]; then exit 0; fi

DEFAULT_DATA="${XDG_DATA_HOME:-$HOME/.local/share}/space.mkv.nuvio.native"
if [ "${NUVIO_UI_PREVIEW:-0}" = 1 ]; then
  DEFAULT_DATA="${DEFAULT_DATA}-preview"
fi
if [ "$RUST_TEST" = 1 ]; then
  # data_init appends the test package ID to this parent, including overrides.
  DEFAULT_DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
  if [ "${NUVIO_UI_PREVIEW:-0}" = 1 ]; then
    DEFAULT_DATA="$DEFAULT_DATA/nuvio-rust-preview"
  fi
fi
export NUVIO_DADOS="${NUVIO_DATA:-${NUVIO_DADOS:-$DEFAULT_DATA}}"
mkdir -p "$NUVIO_DADOS"
cleanup
trap - EXIT
exec "$OUTPUT" "$ART_ROOT" "$@"
