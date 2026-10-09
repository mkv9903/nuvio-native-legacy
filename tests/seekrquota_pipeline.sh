#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota-pipeline.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/seekrquota.c src/data.c src/seekrvtt.c src/js.c src/jpegrapido.c \
  tests/seekrquota_pipeline.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -framework OpenGL -pthread -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined \
  -o /tmp/nuvio-seekrquota-pipeline
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-pipeline
