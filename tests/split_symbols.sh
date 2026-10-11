#!/bin/bash
# Stripping must preserve runtime code/data and leave matching offline symbols.
set -euo pipefail
cd "$(dirname "$0")/.."
scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-symbol-test.XXXXXXXX")
trap 'rm -rf "$scratch"' EXIT
cat > "$scratch/sample.c" <<'C'
__attribute__((noinline)) int crash_capture(int n) { return n + 7; }
int main(int argc, char **argv) { (void)argv; return crash_capture(argc) == 8 ? 0 : 1; }
C
"${CC:-cc}" -O2 -g -Wl,--build-id=sha1 -Wl,--export-dynamic "$scratch/sample.c" -o "$scratch/original"
export NUVIO_ARTIFACT_PLATFORM=test NUVIO_ARTIFACT_VERSION=1.2.3
export NUVIO_BUILD_COMMIT=abc123 NUVIO_BUILD_DIRTY=1
for mode in release debug; do
  cp "$scratch/original" "$scratch/app"
  archive=$(bash tools/split-symbols.sh "$scratch/app" "$scratch/archive" "$mode")
  "$scratch/app"
  cmp "$scratch/app" "$archive/app"
  [ "$(stat -c %a "$scratch/app")" = 755 ]
  pc=$(nm "$archive/app.debug" | awk '$3 == "crash_capture" {print $1}')
  addr2line -e "$archive/app.debug" "$pc" | grep -q 'sample.c:'
  python3 - "$scratch/original" "$archive" "$mode" <<'PY'
import hashlib, json, pathlib, struct, subprocess, sys, zlib
original, archive, mode = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.argv[3]
def elf(file):
    data = file.read_bytes()
    assert data[:4] == b'\x7fELF'
    endian = '<' if data[5] == 1 else '>'
    wide = data[4] == 2
    header = struct.unpack_from(endian + ('HHIQQQIHHHHHH' if wide else 'HHIIIIIHHHHHH'), data, 16)
    phoff, shoff, phsize, phcount, shsize, shcount, namesindex = header[4], header[5], *header[8:13]
    sections = [struct.unpack_from(endian + ('IIQQQQIIQQ' if wide else 'IIIIIIIIII'), data, shoff + i * shsize) for i in range(shcount)]
    names = sections[namesindex]
    names = data[names[4]:names[4] + names[5]]
    contents, allocated = {}, {}
    for section in sections:
        name = names[section[0]:].split(b'\0', 1)[0].decode()
        payload = data[section[4]:section[4] + section[5]] if section[1] != 8 else b''
        contents[name] = payload
        if section[2] & 2:
            allocated[name] = (section[1:7], section[8:], payload)
    loads = []
    for i in range(phcount):
        ph = data[phoff + i * phsize:phoff + (i + 1) * phsize]
        if struct.unpack_from(endian + 'I', ph)[0] == 1:
            loads.append(ph)
    return endian, contents, allocated, loads
before, after = elf(original), elf(archive / 'app')
assert before[2:] == after[2:], 'runtime sections or load mappings changed'
assert '.debug_info' in after[1] if mode == 'debug' else '.debug_info' not in after[1]
assert len((archive / 'app').read_bytes()) < original.stat().st_size if mode == 'release' else True
debug = archive / 'app.debug'
assert '.debug_info' in elf(debug)[1]
link = after[1]['.gnu_debuglink']
name = link.split(b'\0', 1)[0]
assert name == b'app.debug'
crc_offset = (len(name) + 1 + 3) & ~3
assert struct.unpack_from(after[0] + 'I', link, crc_offset)[0] == zlib.crc32(debug.read_bytes())
def build_id(file):
    notes = subprocess.check_output(['readelf', '-n', str(file)], text=True, stderr=subprocess.DEVNULL)
    return next(line.split('Build ID:')[1].strip() for line in notes.splitlines() if 'Build ID:' in line)
metadata = json.loads((archive / 'build.json').read_text())
assert metadata['build_id'] == build_id(original) == build_id(archive / 'app') == build_id(debug)
assert metadata['profile'] == mode and metadata['dirty'] is True
assert metadata['platform'] == 'test' and metadata['version'] == '1.2.3'
assert metadata['git_commit'] == 'abc123'
assert metadata['sha256'] == hashlib.sha256((archive / 'app').read_bytes()).hexdigest()
assert metadata['debug_sha256'] == hashlib.sha256(debug.read_bytes()).hexdigest()
PY
done
# Reject inputs that cannot be matched to an archived build.
"${CC:-cc}" -Wl,--build-id=none "$scratch/sample.c" -o "$scratch/no-id"
if bash tools/split-symbols.sh "$scratch/no-id" "$scratch/archive" release > /dev/null 2>&1; then
  echo 'FAIL: accepted executable without build ID' >&2; exit 1
fi
if bash tools/split-symbols.sh "$scratch/original" "$scratch/archive" invalid > /dev/null 2>&1; then
  echo 'FAIL: accepted invalid profile' >&2; exit 1
fi
echo 'symbol split: PASS (runtime unchanged, build IDs, debug link, offline locations, hashes, profiles)'
