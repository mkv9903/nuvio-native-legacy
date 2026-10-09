#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota-settings.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/data.c tests/seekrquota_settings.c -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 \
  -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-deprecated-declarations -Wno-macro-redefined -o /tmp/nuvio-seekrquota-settings
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-settings
