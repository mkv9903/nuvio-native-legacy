#!/usr/bin/env bash
# Run Rust UI regression tests against the real C screens under Linux/Xvfb.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
directory="$root/build/backend/native-ui"
mkdir -p "$directory"
read -r -a cflags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
read -r -a libraries <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
"${CC:-cc}" -shared -fPIC src/*.c tests/support/rust_addon_views.c tests/support/native_addon_workers.c tests/support/native_titles.c -Isrc -o "$directory/libnuvio_c_ui.so" \
  -O1 -g -DNV_LINUX_DESKTOP -DNV_NATIVE_UI_TEST -DNV_PROFILE_PICKER_TEST \
  '-DNV_APP_ID="space.mkv.nuvio.rusttest"' "${cflags[@]}" "${libraries[@]}" -ldl -pthread -lm
export CARGO_TARGET_DIR="$root/build/backend/ui-target"
export LD_LIBRARY_PATH="$directory${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export RUSTFLAGS="${RUSTFLAGS:-} -L native=$directory -C link-arg=-Wl,--export-dynamic"
export NUVIO_UI_TEST_CAPTURE_DIR="$directory/screens"
mkdir -p "$NUVIO_UI_TEST_CAPTURE_DIR"
cd backend
xvfb-run -a -s '-screen 0 1920x1080x24' cargo test --locked --features native-ui-tests --test native_ui --test native_addons --test native_home_order --test native_titles -- --test-threads=1
