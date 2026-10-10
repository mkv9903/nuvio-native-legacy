#!/bin/bash
# Seekr contra a API real (precisa de rede). Ver tests/seekr_vivo.c.
set -eu
cd "$(dirname "$0")/.."
cc src/seekr.c src/seekrquota.c src/data.c src/seekrvtt.c src/js.c src/net.c src/net_url.c src/jpegrapido.c tests/seekr_vivo.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -o /tmp/nuvio-seekr-vivo -O1 -g \
  -Wno-deprecated-declarations -Wno-macro-redefined
quota_dir=$(mktemp -d /tmp/nuvio-seekr-vivo.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekr-vivo
