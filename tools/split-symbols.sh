#!/bin/bash
# Archive matching symbols before producing the executable that will ship.
# Prints the artifact directory; build/ is ignored by Git and never packaged.
set -euo pipefail
if [ "$#" -ne 3 ] || { [ "$3" != release ] && [ "$3" != debug ]; }; then
  echo 'Usage: split-symbols.sh <linked executable> <archive base> <release|debug>' >&2
  exit 2
fi
binary=$1
archive_base=$2
profile=$3
nv_objcopy=${NUVIO_OBJCOPY:-objcopy}
nv_strip=${NUVIO_STRIP:-strip}
nv_readelf=${NUVIO_READELF:-readelf}
[ -f "$binary" ] || { echo "split-symbols: missing executable: $binary" >&2; exit 1; }
build_id=$("$nv_readelf" -n "$binary" | awk '/Build ID:/ {print $3; exit}')
if [[ ! "$build_id" =~ ^[0-9a-f]+$ ]]; then
  echo 'split-symbols: executable has no build ID; link with -Wl,--build-id=sha1' >&2
  exit 1
fi
artifact_dir="$archive_base/$build_id/$profile"
name=$(basename "$binary")
mkdir -p "$artifact_dir"
cp "$binary" "$artifact_dir/$name"
"$nv_objcopy" --only-keep-debug "$artifact_dir/$name" "$artifact_dir/$name.debug"
if [ "$profile" = release ]; then
  "$nv_strip" --strip-unneeded "$artifact_dir/$name"
fi
"$nv_objcopy" --add-gnu-debuglink="$artifact_dir/$name.debug" "$artifact_dir/$name"
chmod 755 "$artifact_dir/$name"
# Record only identifiers and hashes, never compiler flags or credentials.
python3 - "$artifact_dir" "$name" "$build_id" "$profile" <<'PY'
import hashlib, json, os, pathlib, sys
directory, name, build_id, profile = sys.argv[1:]
directory = pathlib.Path(directory)
def sha256(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()
metadata = {
    'build_id': build_id, 'profile': profile,
    'platform': os.environ.get('NUVIO_ARTIFACT_PLATFORM', ''),
    'version': os.environ.get('NUVIO_ARTIFACT_VERSION', ''),
    'git_commit': os.environ.get('NUVIO_BUILD_COMMIT', ''),
    'dirty': os.environ.get('NUVIO_BUILD_DIRTY', '0') == '1',
    'executable': name, 'sha256': sha256(directory / name),
    'debug_file': name + '.debug', 'debug_sha256': sha256(directory / (name + '.debug')),
}
(directory / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
PY
cp "$artifact_dir/$name" "$binary"
chmod 755 "$binary"
printf '%s\n' "$artifact_dir"
