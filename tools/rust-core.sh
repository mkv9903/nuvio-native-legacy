#!/bin/bash
# Build the pinned Rust static library; stdout contains only its absolute path.
set -euo pipefail
cd "$(dirname "$0")/.."
rust_profile=release
rust_target=''
while [ "$#" -gt 0 ]; do
  case "$1" in
    --target) rust_target=$2; shift 2 ;;
    --debug) rust_profile=debug; shift ;;
    *) echo "rust-core: unknown option: $1" >&2; exit 2 ;;
  esac
done
rust_toolchain=$(sed -n 's/^channel = "\([^"]*\)"/\1/p' rust/rust-toolchain.toml)
[ -n "$rust_toolchain" ] || { echo 'rust-core: missing pinned toolchain' >&2; exit 2; }
export CARGO_TARGET_DIR="$PWD/build/rust/target"
export CARGO_HOME="${NUVIO_CARGO_HOME:-$PWD/build/rust/cargo-home}"
rust_args=(build --locked --manifest-path rust/Cargo.toml -p nuvio-core)
if [ "$rust_profile" = release ]; then rust_args+=(--release); fi
if [ -n "$rust_target" ]; then rust_args+=(--target "$rust_target"); fi
rustup run "$rust_toolchain" cargo "${rust_args[@]}" >&2
rust_output="$CARGO_TARGET_DIR"
if [ -n "$rust_target" ]; then rust_output="$rust_output/$rust_target"; fi
printf '%s/%s/libnuvio_core.a\n' "$rust_output" "$rust_profile"
