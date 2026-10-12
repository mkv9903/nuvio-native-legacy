#!/bin/bash
# Independent Rust app commands. Never builds or links src/*.c or nuvio-core.
set -euo pipefail
standalone_root=$(cd "$(dirname "$0")/.." && pwd)
cd "$standalone_root/standalone"
export CARGO_HOME="${NUVIO_CARGO_HOME:-$standalone_root/build/rust/cargo-home}"
export CARGO_TARGET_DIR="$standalone_root/build/standalone/target"
standalone_toolchain=1.85.1
standalone_command=${1:-linux}
if [ "$#" -gt 0 ]; then shift; fi
case "$standalone_command" in
  check)
    rustup run "$standalone_toolchain" cargo test --locked "$@"
    ;;
  build)
    rustup run "$standalone_toolchain" cargo build --locked "$@"
    ;;
  linux)
    rustup run "$standalone_toolchain" cargo build --locked
    exec "$CARGO_TARGET_DIR/debug/nuvio-rust-app" "$@"
    ;;
  arm)
    mkdir -p "$standalone_root/build/standalone/tmp"
    export TMPDIR="$standalone_root/build/standalone/tmp"
    export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABI_LINKER="$standalone_root/tools/standalone-sdk-gcc.sh"
    export CC_armv7_unknown_linux_gnueabi="$standalone_root/tools/standalone-sdk-gcc.sh"
    export AR_armv7_unknown_linux_gnueabi="$standalone_root/tools/standalone-sdk-ar.sh"
    export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABI_RUSTFLAGS='-C link-arg=-Wl,--build-id=sha1'
    rustup run "$standalone_toolchain" cargo build --locked --release --features webos --target armv7-unknown-linux-gnueabi "$@"
    ;;
  *)
    echo 'Usage: bash tools/rust-app.sh {check|build|arm} [Cargo arguments] | linux [app arguments]' >&2
    exit 2
    ;;
esac
