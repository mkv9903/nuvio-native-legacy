#!/bin/bash
set -euo pipefail
standalone_root=$(cd "$(dirname "$0")/.." && pwd)
exec "${NUVIO_CONTAINER_RUNTIME:-docker}" run --rm --platform linux/arm64 --network none \
  -v "$standalone_root:$standalone_root" -w "$PWD" \
  "${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}" arm-webos-linux-gnueabi-ar "$@"
