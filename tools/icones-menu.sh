#!/bin/bash
# SIDEBAR ICONS (Modern and Default layouts), thin line stroke.
#
# Rasterization matches tools/icones-lucide.sh (512 via ImageMagick, downsampled to
# 128 via Box filter, shape in ALPHA, white).
#
# Usage: bash tools/icones-menu.sh     Requires: ImageMagick (magick or convert).
set -e
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"
DIR="deploy/app/art/icones"
SVG="$DIR/menu-traco"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for f in "$SVG"/*.svg; do
  n=$(basename "$f" .svg)
  sed -e 's/currentColor/#ffffff/g' -e 's/width="24"/width="512"/' \
      -e 's/height="24"/height="512"/' "$f" > "$TMP/a.svg"
  "$IMAGE_TOOL" -background none "$TMP/a.svg" "$TMP/a.png" >/dev/null
  "$IMAGE_TOOL" "$TMP/a.png" -alpha extract -filter Box -resize 128x128 "$TMP/m.png"
  "$IMAGE_TOOL" -size 128x128 xc:white "$TMP/m.png" -alpha off -compose CopyOpacity \
    -composite -strip -define png:color-type=6 "$DIR/mt_$n.png"
done
echo "icones-menu: $(ls "$SVG"/*.svg | wc -l | tr -d ' ') icons in $DIR/mt_*.png"
