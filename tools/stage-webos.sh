#!/bin/bash
# Assemble only distributable app resources. Sample catalogs/artwork and local
# account data remain available to desktop previews, never to a TV package.
set -euo pipefail
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
  echo "Usage: bash tools/stage-webos.sh <source app directory> <new destination> [executable]" >&2
  exit 2
fi
source_app=$1
payload_app=$2
executable=${3:-$source_app/nuvio-proto}
[ -f "$source_app/appinfo.json" ] && [ -f "$executable" ] || {
  echo "stage-webos: source app metadata or executable is missing" >&2
  exit 1
}
[ ! -e "$payload_app" ] || {
  echo "stage-webos: destination must not already exist" >&2
  exit 1
}
mkdir -p "$payload_app/art"
for resource in appinfo.json icon.png icon-large.png icon-20.png icon-large-20.png splash.png fonts; do
  cp -R "$source_app/$resource" "$payload_app/$resource"
done
cp "$executable" "$payload_app/nuvio-proto"
# Preserve license notices; the retired conversion license bundle is omitted.
if [ -d "$source_app/licencas" ]; then
  cp -R "$source_app/licencas" "$payload_app/licencas"
fi
for resource in badges icones icones-app marcas selos addons-recomendados.txt; do
  cp -R "$source_app/art/$resource" "$payload_app/art/$resource"
done
chmod 755 "$payload_app/nuvio-proto"
