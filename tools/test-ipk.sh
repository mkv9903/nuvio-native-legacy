#!/bin/bash
# Verifies that .ipk does NOT contain user credentials — without requiring docker.
#
# Packages the same allowlisted payload as arm.sh, using the executable already
# present in deploy/app, without recompiling or contacting a TV.
#
# TWO PITFALLS this test prevents:
#   1. `ares-package deploy/app` includes entire directory, and art/ contains Trakt
#      tokens, addon URLs with debrid keys, TMDB key, and mdblist key.
#   2. .ipk is a Debian package: `tar tzf package.ipk` lists only
#      debian-binary / control.tar.gz / data.tar.gz without error.
#      A check written this way always passes even when secrets are inside.
#      Must unpack `ar` and inspect data.tar.gz.
set -e
cd "$(dirname "$0")/.."
if [ -n "${NUVIO_ARES_PACKAGE:-}" ]; then
  ARES="$NUVIO_ARES_PACKAGE"
elif [ -x "../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package" ]; then
  ARES="../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package"
else
  ARES="$(command -v ares-package || echo ares-package)"
fi
STAGING=$(mktemp -d)
trap 'rm -rf "$STAGING"' EXIT
bash tools/stage-webos.sh deploy/app "$STAGING/app"

OUTPUT_DIR="$STAGING/packages"
mkdir "$OUTPUT_DIR"
"$ARES" "$STAGING/app" -o "$OUTPUT_DIR" >/dev/null
PACKAGES=("$OUTPUT_DIR/"*.ipk)
if [ "${#PACKAGES[@]}" -ne 1 ] || [ ! -f "${PACKAGES[0]}" ]; then
  echo 'ABORTED: expected exactly one generated IPK' >&2; exit 1
fi
IPK=${PACKAGES[0]}

PKG_LIST=$(cd "$STAGING" && ar x "$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
[ -z "$PKG_LIST" ] && { echo "ABORTED: could not read package to verify"; exit 1; }

echo "test package: $(du -h "$IPK" | cut -f1)"
python3 - "$STAGING/app" "$STAGING/data.tar.gz" <<'PY'
import json, pathlib, sys, tarfile
source = pathlib.Path(sys.argv[1])
prefix = 'usr/palm/applications/space.mkv.nuvio.native/'
expected = {prefix + p.relative_to(source).as_posix(): p for p in source.rglob('*') if p.is_file()}
package_info = 'usr/palm/packages/space.mkv.nuvio.native/packageinfo.json'
app_info = json.loads((source / 'appinfo.json').read_text())
found_info = False
# Stream once; repeatedly seeking in gzip would decompress the app hundreds of times.
with tarfile.open(sys.argv[2], mode='r|gz') as archive:
    for member in archive:
        if member.isdir():
            continue
        assert member.isfile(), 'unexpected link or special file'
        name = member.name.removeprefix('./')
        assert not name.endswith('.debug') and not name.endswith('/build.json')
        if name == package_info:
            assert not found_info, 'duplicate package metadata'
            info = json.load(archive.extractfile(member))
            assert info['id'] == app_info['id'] and info['version'] == app_info['version']
            found_info = True
            continue
        assert name in expected, 'file outside allowlisted payload: ' + name
        assert archive.extractfile(member).read() == expected.pop(name).read_bytes(), name
        if name == prefix + 'nuvio-proto':
            assert member.mode == 0o755, 'executable permissions changed'
assert found_info and not expected, 'missing package metadata or runtime files'
PY
echo "OK: exact allowlisted payload, executable mode, no private/sample data or symbol files"
