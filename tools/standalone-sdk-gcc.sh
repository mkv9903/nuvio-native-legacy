#!/bin/bash
# SDK compiler/linker for standalone Rust and third-party native dependencies.
# No application C files are compiled. Cargo's paths remain valid in the SDK.
set -euo pipefail
standalone_root=$(cd "$(dirname "$0")/.." && pwd)
standalone_rustup=${RUSTUP_HOME:-$HOME/.rustup}
exec "${NUVIO_CONTAINER_RUNTIME:-docker}" run --rm --platform linux/arm64 --network none \
  -v "$standalone_root:$standalone_root" \
  -v "$standalone_rustup:$standalone_rustup:ro" \
  -w "$PWD" "${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}" \
  arm-webos-linux-gnueabi-gcc "$@"
