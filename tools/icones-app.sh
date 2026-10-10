#!/bin/bash
# ALTERNATIVE APP ICONS (local gallery, see src/iconeapp.h).
#
# Source: design/icones-apoio/1..7 (owner artwork, unedited). Sheet 7 contains
# three sticker logos with labels; this script crops each sticker without the label,
# removing dark sheet background.
#
# Outputs (all regenerable; idempotent):
#   design/icones-apoio/recortes/7{a,b,c}.png  three stickers, transparent background
#   deploy/app/art/icones-app/<id>.png        256x256, BRAND with transparent background,
#                                             matching original framing (brand occupies
#                                             78% of height, centered).
#
# Original follows current deploy/app/icon.png branding.
# Package launcher assets remain unchanged.
#
# Requires ImageMagick (magick or convert).
set -euo pipefail
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"
SRC=design/icones-apoio
REC=$SRC/recortes
ART=deploy/app/art/icones-app
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$REC" "$ART"

# --- Crop sheet 7 -------------------------------------------------------------
"$IMAGE_TOOL" "$SRC/7.webp" "$TMP/folha.png"
for spec in "a 25 60 590 690" "b 612 90 650 650" "c 1262 100 615 640"; do
  set -- $spec
  "$IMAGE_TOOL" "$TMP/folha.png" -crop "${4}x${5}+${2}+${3}" +repage \
    -bordercolor 'rgb(23,24,31)' -border 6 "$TMP/c$1.png"
  "$IMAGE_TOOL" "$TMP/c$1.png" -fuzz 7% -fill black -opaque 'rgb(23,24,31)' \
    -fill white +opaque black -morphology Close Disk:2 \
    -fill gray50 -draw 'color 0,0 floodfill' -fill white +opaque gray50 \
    -fill black -opaque gray50 -morphology Erode Disk:1 -blur 0x0.8 \
    -strip -colorspace gray "$TMP/m$1.png"
  "$IMAGE_TOOL" "$TMP/c$1.png" "$TMP/m$1.png" -alpha off -compose CopyOpacity \
    -composite -trim +repage -strip "$REC/7$1.png"
done

# Preserve current package branding; Original is never regenerated from old logo.
"$IMAGE_TOOL" deploy/app/icon.png -resize 256x256 -strip "$ART/original.png"

# Alternative IDs match src/iconeapp.c; Original generated above.
LISTA="fenix:$SRC/1.webp nverde:$SRC/2.webp
tvlaranja:$SRC/3.webp npixel:$SRC/4.png tricolor:$SRC/5.webp arco:$SRC/6.png
tvviva:$REC/7a.png cluberetro:$REC/7b.png arcaden:$REC/7c.png"

# Brand centered in square of size L occupying `pct` percentage of side.
center_brand() {  # source side pct bg output
  local target=$(( $2 * $3 / 100 ))
  "$IMAGE_TOOL" "$1" -trim +repage -resize "${target}x${target}" -background "$4" \
    -gravity center -extent "$2x$2" "$5"
}

for item in $LISTA; do
  id=${item%%:*}; f=${item#*:}
  center_brand "$f" 256 78 none "$TMP/$id.png"
  "$IMAGE_TOOL" "$TMP/$id.png" -strip -define png:compression-level=9 "$ART/$id.png"
done
du -ch "$ART"/*.png 2>/dev/null | tail -1 || true
