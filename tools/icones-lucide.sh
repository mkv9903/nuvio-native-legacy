#!/bin/bash
# SETTINGS icons, from Lucide (lucide.dev, ISC license — text in
# deploy/app/art/icones/lucide/LICENSE).
#
# Usage: bash tools/icones-lucide.sh            (download and rasterize)
#      bash tools/icones-lucide.sh --offline  (rasterize cached SVGs only)
# Requires: curl, ImageMagick (magick or convert).
set -e
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"

TAG="1.48.0"
BASE="https://raw.githubusercontent.com/lucide-icons/lucide/$TAG/icons"
DIR="deploy/app/art/icones"
SVG="$DIR/lucide"

NOMES="
circle-play file-text folders house
images info layout-dashboard monitor-cog palette panel-top plug puzzle
rotate-ccw-clock rows-3 star user-round mic keyboard smartphone
rotate-cw x library eye eye-off users
clock triangle-alert tv-minimal-play calendar download wifi-off wifi link-2-off
cloud megaphone bookmark
clapperboard languages search chevron-right check plus minus memory-stick delete
activity gauge zap database
arrow-up-down heart moon sun-dim
arrow-up arrow-down pencil
pause send scroll-text server-crash shield-check cpu log-out
panel-left list-x bell audio-lines layers type sliders-horizontal user-plus
alarm-clock badge-check captions chart-column chevrons-down circle-check circle-dot compass droplet feather film funnel globe image image-plus inbox key key-round layout-list life-buoy link list-ordered list-video loader log-in map maximize-2 menu message-square monitor-play mouse-pointer move-horizontal pause picture-in-picture-2 play quote send server settings-2 shield sparkle stethoscope thumbs-up unlink wand sparkles corner-up-left
pipette blend spotlight paint-roller
ellipsis
hard-drive volume-2 package tag
"

mkdir -p "$SVG"
if [ "${1:-}" != "--offline" ]; then
  curl -fsSL -o "$SVG/LICENSE" "https://raw.githubusercontent.com/lucide-icons/lucide/$TAG/LICENSE"
  for n in $NOMES; do
    curl -fsSL -o "$SVG/$n.svg" "$BASE/$n.svg" || { echo "icones-lucide: $n not found in $TAG" >&2; exit 1; }
  done
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for n in $NOMES; do
  sed -e 's/currentColor/#ffffff/g' -e 's/width="24"/width="512"/' \
      -e 's/height="24"/height="512"/' "$SVG/$n.svg" > "$TMP/a.svg"
  "$IMAGE_TOOL" -background none "$TMP/a.svg" "$TMP/a.png" >/dev/null
  "$IMAGE_TOOL" "$TMP/a.png" -alpha extract -filter Box -resize 128x128 "$TMP/m.png"
  "$IMAGE_TOOL" -size 128x128 xc:white "$TMP/m.png" -alpha off -compose CopyOpacity \
    -composite -strip -define png:color-type=6 "$DIR/aj_$n.png"
done

# Prune unused icons so package doesn't carry orphaned files.
for f in "$DIR"/aj_*.png "$SVG"/*.svg; do
  n=$(basename "$f"); n=${n#aj_}; n=${n%.png}; n=${n%.svg}
  case " $(echo $NOMES) " in *" $n "*) ;; *) rm -f "$f"; echo "icones-lucide: removed $f" ;; esac
done

USADOS=$(grep -ho '"aj_[a-z0-9-]*"' src/*.c src/*.inc 2>/dev/null | tr -d '"' | sed 's/^aj_//' | sort -u)
FAIL=0
for n in $USADOS; do
  case " $(echo $NOMES) " in *" $n "*) ;; *) echo "icones-lucide: src/ uses aj_$n, missing from NOMES" >&2; FAIL=1 ;; esac
done
for n in $NOMES; do
  printf '%s\n' "$USADOS" | grep -qx "$n" || { echo "icones-lucide: aj_$n is unused in src/" >&2; FAIL=1; }
done
[ "$FAIL" = 0 ] || exit 1
echo "icones-lucide: $(echo $NOMES | wc -w | tr -d ' ') icons in $DIR/aj_*.png (Lucide $TAG)"
