#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota-pipeline.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/seekrquota.c src/data.c src/seekrvtt.c src/js.c src/jpegrapido.c \
  tests/seekrquota_pipeline.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -pthread -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  -o /tmp/nuvio-seekrquota-pipeline
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-pipeline
