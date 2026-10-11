#!/bin/bash
# Rust units, full-scalar C ABI parity, then the retained regression fixtures.
set -euo pipefail
cd "$(dirname "$0")/.."
text_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-text.XXXXXXXX")
trap 'rm -rf "$text_scratch"' EXIT
export CARGO_TARGET_DIR="$PWD/build/rust/target"
export CARGO_HOME="${NUVIO_CARGO_HOME:-$PWD/build/rust/cargo-home}"
toolchain=$(sed -n 's/^channel = "\([^"]*\)"/\1/p' rust/rust-toolchain.toml)
rustup run "$toolchain" cargo test --locked --offline --manifest-path rust/Cargo.toml -p nuvio-core
export NUVIO_TEXT_RUST_LIBRARY
NUVIO_TEXT_RUST_LIBRARY=$(bash tools/rust-core.sh)
flags=(-O1 -g -Isrc -std=c11 -DNV_RUST_CORE=1 -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/text_reference.c tests/rust_text_helpers.c src/dobra.c src/limpa.c src/buscanorm.c \
  "$NUVIO_TEXT_RUST_LIBRARY" -ldl -lpthread -lm -lrt -o "$text_scratch/parity"
"$text_scratch/parity"
for name in dobra limpa buscanorm; do
  bash "tests/$name.sh"
  bash "tests/$name.sh" --rust
done
