#!/bin/bash
# PLAYER icons in Glass UI (approved mockup from 10/03, player-mockup.html).
#
# Rasterizes each into deploy/app/art/icones/pl_<nome>.png, 128x128, WHITE
# on transparent with shape in ALPHA. Prefix pl_ separates them from other screens.
#
# Usage: bash tools/icones-player.sh      Requires: sips (macOS), magick.
set -e
cd "$(dirname "$0")/.."
DIR="deploy/app/art/icones"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for f in "$DIR"/player/*.svg; do
  n=$(basename "$f" .svg)
  sed -e 's/currentColor/#ffffff/g' -e 's/width="24"/width="512"/' \
      -e 's/height="24"/height="512"/' "$f" > "$TMP/a.svg"
  sips -s format png "$TMP/a.svg" --out "$TMP/a.png" >/dev/null
  magick "$TMP/a.png" -alpha extract -filter Box -resize 128x128 "$TMP/m.png"
  magick -size 128x128 xc:white "$TMP/m.png" -alpha off -compose CopyOpacity \
    -composite -strip -define png:color-type=6 "$DIR/pl_$n.png"
done
USADOS=$(grep -ho '"pl_[a-z0-9-]*"' src/*.c 2>/dev/null | tr -d '"' | sed 's/^pl_//' | sort -u)
for n in $USADOS; do
  [ -f "$DIR/player/$n.svg" ] || { echo "icones-player: src/ uses pl_$n without SVG" >&2; exit 1; }
done
echo "icones-player: $(ls "$DIR"/player/*.svg | wc -l | tr -d ' ') icons in $DIR/pl_*.png"
