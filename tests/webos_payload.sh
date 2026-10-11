#!/bin/bash
# The TV payload must preserve runtime UI resources without sample/private data.
set -euo pipefail
cd "$(dirname "$0")/.."
scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-webos-payload.XXXXXXXX")
trap 'rm -rf "$scratch"' EXIT
source_app="$scratch/source"
mkdir -p "$source_app/art" "$source_app/fonts" "$source_app/licencas"
for file in appinfo.json icon.png icon-large.png icon-20.png icon-large-20.png splash.png nuvio-proto; do
  printf 'runtime resource\n' > "$source_app/$file"
done
printf 'font\n' > "$source_app/fonts/interface.ttf"
printf 'license\n' > "$source_app/licencas/library.txt"
for dir in badges icones icones-app marcas selos; do
  mkdir -p "$source_app/art/$dir"
  printf 'UI asset\n' > "$source_app/art/$dir/asset.png"
done
printf 'curated addon suggestions\n' > "$source_app/art/addons-recomendados.txt"
for dir in cinematic editorial poster logo ep elenco prov collections cache; do
  mkdir -p "$source_app/art/$dir"
  printf 'sample or private content\n' > "$source_app/art/$dir/asset.jpg"
done
for file in 00.jpg 39.jpg catalogo.txt ids.txt elenco.txt episodios.txt extra.txt LEIA-ME-addons.txt collections.json catalogo-rede.bin sessao.txt ajustes.txt conta-user.txt jellyfin-p1.txt future-private-data.txt; do
  printf 'sample or private content\n' > "$source_app/art/$file"
done
mkdir -p "$source_app/lib" "$source_app/licenses/dts"
printf 'retired converter\n' > "$source_app/lib/dts-starfish-webos.so"
mkdir -p "$source_app/build/symbols"
for file in nuvio-proto.debug nuvio-proto.full build.json build/symbols/app.debug; do
  printf 'developer artifact\n' > "$source_app/$file"
done

bash tools/stage-webos.sh "$source_app" "$scratch/payload"
python3 - "$source_app" "$scratch/payload" <<'PY'
import pathlib, sys
source, payload = map(pathlib.Path, sys.argv[1:])
allowed_root = {'appinfo.json', 'icon.png', 'icon-large.png', 'icon-20.png',
                'icon-large-20.png', 'splash.png', 'nuvio-proto', 'fonts', 'licencas', 'art'}
allowed_art = {'badges', 'icones', 'icones-app', 'marcas', 'selos', 'addons-recomendados.txt'}
assert {p.name for p in payload.iterdir()} == allowed_root
assert {p.name for p in (payload / 'art').iterdir()} == allowed_art
for file in payload.rglob('*'):
    if file.is_file():
        assert file.read_bytes() == (source / file.relative_to(payload)).read_bytes()
assert (payload / 'nuvio-proto').stat().st_mode & 0o777 == 0o755
assert (source / 'art/collections/asset.jpg').exists()
assert (source / 'art/future-private-data.txt').exists()
PY
# Existing destinations cannot silently retain resources from an earlier build.
if bash tools/stage-webos.sh "$source_app" "$scratch/payload" > /dev/null 2>&1; then
  echo 'FAIL: staging accepted an existing destination' >&2
  exit 1
fi
# Isolated variants can stage a different executable and metadata without
# replacing or depending on a production executable in the resource directory.
printf '{"id":"space.mkv.nuvio.rusttest","title":"Nuvio Rust Test"}\n' > "$scratch/rust-appinfo.json"
printf 'isolated executable\n' > "$scratch/rust-executable"
rm "$source_app/nuvio-proto"
NUVIO_STAGE_APPINFO="$scratch/rust-appinfo.json" NUVIO_STAGE_BINARY="$scratch/rust-executable" \
  bash tools/stage-webos.sh "$source_app" "$scratch/rust-payload"
cmp "$scratch/rust-appinfo.json" "$scratch/rust-payload/appinfo.json"
cmp "$scratch/rust-executable" "$scratch/rust-payload/nuvio-proto"
[ "$(cat "$source_app/appinfo.json")" = 'runtime resource' ]
printf '{"id":"space.mkv.nuvio.native","title":"Nuvio Native"}\n' > "$source_app/appinfo.json"
bash tools/stage-rust-preview.sh "$source_app" "$scratch/rust-executable" "$scratch/preview-payload"
python3 - "$scratch/preview-payload" <<'PY'
import json, pathlib, sys
preview = pathlib.Path(sys.argv[1])
assert json.loads((preview / 'appinfo.json').read_text())['id'] == 'space.mkv.nuvio.rusttest'
assert (preview / 'art/catalogo.txt').is_file()
assert (preview / 'art/00.jpg').is_file()
assert (preview / 'art/poster/asset.jpg').is_file()
assert (preview / 'licencas/rust/COPYRIGHT').is_file()
for name in ['collections.json', 'catalogo-rede.bin', 'sessao.txt', 'ajustes.txt',
             'conta-user.txt', 'jellyfin-p1.txt', 'future-private-data.txt', 'cache']:
    assert not (preview / 'art' / name).exists(), name
PY
echo 'webOS payload: PASS (UI resources, no sample/private data, unchanged source)'
echo 'Rust preview payload: PASS (isolated identity, sample UI content, no account/settings/cache files)'
