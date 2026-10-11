#!/bin/bash
# Writer regressions plus existing account/sync/authentication consumers.
set -euo pipefail
cd "$(dirname "$0")/.."
json_writer_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-json-writer.XXXXXXXX")
trap 'rm -rf "$json_writer_scratch"' EXIT
export CARGO_TARGET_DIR="$PWD/build/rust/target"
export CARGO_HOME="${NUVIO_CARGO_HOME:-$PWD/build/rust/cargo-home}"
toolchain=$(sed -n 's/^channel = "\([^"]*\)"/\1/p' rust/rust-toolchain.toml)
rustup run "$toolchain" cargo test --locked --offline --manifest-path rust/Cargo.toml -p nuvio-core
export NUVIO_JSON_RUST_LIBRARY
NUVIO_JSON_RUST_LIBRARY=$(bash tools/rust-core.sh)
bash tests/json_writer.sh
bash tests/json_writer.sh --rust

flags=(-O1 -g -Isrc -std=gnu11 -DNV_RUST_CORE=1 -DNV_LINUX_DESKTOP -Wall -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
read -r -a dependency_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
flags+=("${dependency_flags[@]}")
libraries=("$NUVIO_JSON_RUST_LIBRARY" -ldl -lpthread -lm -lrt)
cc "${flags[@]}" src/account_pending.c src/vistoep.c src/js.c src/jsw.c tests/contapend.c "${libraries[@]}" -o "$json_writer_scratch/pending"
"$json_writer_scratch/pending"
cc "${flags[@]}" src/traktauth.c src/js.c src/jsw.c tests/traktauth_renov.c "${libraries[@]}" -o "$json_writer_scratch/trakt-refresh"
"$json_writer_scratch/trakt-refresh"
cc "${flags[@]}" src/traktauth.c src/js.c src/jsw.c tests/traktauth_perfil.c "${libraries[@]}" -o "$json_writer_scratch/trakt-profile"
"$json_writer_scratch/trakt-profile"
cc "${flags[@]}" src/simklauth.c src/js.c src/jsw.c tests/simklauth_perfil.c "${libraries[@]}" -o "$json_writer_scratch/simkl-profile"
"$json_writer_scratch/simkl-profile"
echo 'Rust JSON writer account/sync/authentication integration: ok'
