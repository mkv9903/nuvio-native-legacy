#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota-settings.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/data.c tests/seekrquota_settings.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
    -lSDL2 \
  -O1 -g -ffunction-sections -fdata-sections -Wl,--gc-sections \
  -Wno-deprecated-declarations -Wno-macro-redefined -o /tmp/nuvio-seekrquota-settings
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-settings
