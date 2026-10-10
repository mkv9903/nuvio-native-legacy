#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-mkvass-url.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) tests/video_url.c src/video.c -o "$DIR/video-url"
"$DIR/video-url"
${FFMPEG:-ffmpeg} -v error -y \
  -f lavfi -i 'testsrc2=size=320x180:rate=24:duration=15' \
  -i tests/fixtures/ass/simples.ass -map 0:v -map 1:s \
  -c:v libx264 -c:s ass "$DIR/test.mkv"
cc -Isrc -O1 -g tests/mkvass_url.c src/mkvass.c src/legenda.c src/assrender.c src/data.c \
  -o "$DIR/test"
mkdir "$DIR/dados"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/test.mkv"
