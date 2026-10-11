#!/bin/bash
# Fail before packaging if the hybrid executable violates the SDK ABI limits.
set -euo pipefail
if [ "$#" -ne 1 ]; then
  echo 'Usage: bash tools/inspect-rust-webos.sh <ARM executable>' >&2
  exit 2
fi
python3 - "$1" "${NUVIO_READELF:-readelf}" <<'PY'
import hashlib, json, pathlib, re, subprocess, sys
binary, readelf = sys.argv[1:]
def inspect(flag):
    return subprocess.check_output([readelf, flag, binary], text=True)
header = inspect('-h')
program = inspect('-l')
attributes = inspect('-A')
dynamic = inspect('-d')
versions = inspect('--version-info')
checks = [
    ('32-bit ARMv7', 'ELF32' in header and re.search(r'Machine:\s+ARM\b', header)
     and re.search(r'Tag_CPU_arch:\s+v7\s', attributes)),
    ('soft-float EABI5', 'Version5 EABI, soft-float ABI' in header),
    ('GNU soft-float loader', '[Requesting program interpreter: /lib/ld-linux.so.3]' in program),
]
glibc = sorted(set(re.findall(r'Name: (GLIBC_[0-9.]+)', versions)),
               key=lambda value: tuple(map(int, value[6:].split('.'))))
checks.append(('glibc <= 2.12', glibc and tuple(map(int, glibc[-1][6:].split('.'))) <= (2, 12)))
libraries = re.findall(r'Shared library: \[(.*?)\]', dynamic)
checks.append(('static Rust / dynamic SDK SDL2 and GLES2',
               'libSDL2-2.0.so.0' in libraries and 'libGLESv2.so.2' in libraries
               and not any('rust' in value or 'curl' in value for value in libraries)))
for name, passed in checks:
    if not passed:
        raise SystemExit(f'Rust/webOS inspection failed: {name}')
print(json.dumps({'file': binary, 'sha256': hashlib.sha256(pathlib.Path(binary).read_bytes()).hexdigest(),
                  'abi': 'ARMv7 EABI5 soft-float', 'interpreter': '/lib/ld-linux.so.3',
                  'glibc_versions': glibc, 'needed': libraries,
                  'physical_tv_verified': False}, indent=2))
PY
