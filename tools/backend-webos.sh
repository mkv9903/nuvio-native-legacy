#!/usr/bin/env bash
# Build/package the isolated Rust backend test app. Never installs or launches.
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
package=0
for option in "$@"; do
  case "$option" in
    --build) ;;
    --ipk) package=1 ;;
    --help|-h) echo 'Usage: bash tools/backend-webos.sh [--build] [--ipk]'; exit 0 ;;
    *) echo "Unsupported backend build option: $option" >&2; exit 2 ;;
  esac
done
runtime="${NUVIO_CONTAINER_RUNTIME:-docker}"
export NUVIO_BACKEND_RUNTIME="$runtime"
export NUVIO_BACKEND_SDK="nuvio-backend-sdk-$$"
export CARGO_TARGET_DIR="$root/build/backend/target"
export TMPDIR="$root/build/backend/tmp"
mkdir -p "$TMPDIR" "$root/build/backend/webos"
environment=$(mktemp "$TMPDIR/environment.XXXXXX")
cleanup() { "$runtime" rm -f "$NUVIO_BACKEND_SDK" >/dev/null 2>&1 || true; rm -f "$environment"; }
trap cleanup EXIT
bash tools/env.sh --env-file "$environment"
"$runtime" run --rm -d --platform "${NUVIO_BUILD_PLATFORM:-linux/arm64}" \
  --name "$NUVIO_BACKEND_SDK" --env-file "$environment" \
  -v "$root:$root" -w "$root" "${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}" sleep infinity >/dev/null
export CC_armv7_unknown_linux_gnueabi="$root/tools/backend-arm-cc.sh"
export AR_armv7_unknown_linux_gnueabi="$root/tools/backend-arm-ar.sh"
export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABI_LINKER="$CC_armv7_unknown_linux_gnueabi"
(cd backend; cargo build --locked --release --features webos --target armv7-unknown-linux-gnueabi)
"$runtime" exec "$NUVIO_BACKEND_SDK" bash tools/backend-webos-link.sh
if [ "$package" = 1 ]; then
  stage=$(mktemp -d "$TMPDIR/package.XXXXXX")
  trap 'cleanup; rm -rf "$stage"' EXIT
  # Copy only the established distributable resource allowlist.
  bash tools/stage-webos.sh deploy/app "$stage/app" build/backend/webos/nuvio-rusttest
  python3 - "$stage/app/appinfo.json" <<'PY'
import json, sys
from pathlib import Path
path = Path(sys.argv[1])
metadata = json.loads(path.read_text())
metadata['id'] = 'space.mkv.nuvio.rusttest'
metadata['title'] = 'Nuvio Rust Test'
path.write_text(json.dumps(metadata, indent=2)+'\n')
PY
  "${NUVIO_ARES_PACKAGE:-ares-package}" "$stage/app" -o build/backend/webos
fi
echo 'Isolated webOS build: build/backend/webos/nuvio-rusttest'
