#!/bin/bash
# Keep sample UI content while excluding developer accounts and preferences.
set -euo pipefail
if [ "$#" -ne 3 ]; then
  echo 'Usage: bash tools/stage-rust-preview.sh <source app> <Linux executable> <new destination>' >&2
  exit 2
fi
cd "$(dirname "$0")/.."
preview_source=$1
preview_binary=$2
preview_destination=$3
preview_metadata=$(mktemp "${TMPDIR:-/tmp}/nuvio-rust-preview-meta.XXXXXXXX")
trap 'rm -f "$preview_metadata"' EXIT
python3 - "$preview_source/appinfo.json" "$preview_metadata" <<'PY'
import json, pathlib, sys
source, destination = sys.argv[1:]
metadata = json.loads(pathlib.Path(source).read_text())
metadata.update(id='space.mkv.nuvio.rusttest', title='Nuvio Rust Test')
pathlib.Path(destination).write_text(json.dumps(metadata, indent=2) + '\n')
PY
NUVIO_STAGE_APPINFO="$preview_metadata" NUVIO_STAGE_BINARY="$preview_binary" \
  bash tools/stage-webos.sh "$preview_source" "$preview_destination"
mkdir -p "$preview_destination/licencas/rust"
cp rust/licenses/* "$preview_destination/licencas/rust/"
# Only the existing public preview catalog and artwork are added to the TV
# resource allowlist. Account files, tokens, settings and caches stay excluded.
for preview_resource in catalogo.txt ids.txt elenco.txt episodios.txt extra.txt \
    cinematic editorial poster logo ep elenco prov; do
  if [ -e "$preview_source/art/$preview_resource" ]; then
    cp -R "$preview_source/art/$preview_resource" "$preview_destination/art/"
  fi
done
shopt -s nullglob
for preview_image in "$preview_source"/art/[0-9][0-9].jpg; do
  cp "$preview_image" "$preview_destination/art/"
done
