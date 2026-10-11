#!/bin/bash
# Reader units, C compatibility, approved prefix behavior and real consumers.
set -euo pipefail
cd "$(dirname "$0")/.."
json_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-json.XXXXXXXX")
trap 'rm -rf "$json_scratch"' EXIT
export CARGO_TARGET_DIR="$PWD/build/rust/target"
export CARGO_HOME="${NUVIO_CARGO_HOME:-$PWD/build/rust/cargo-home}"
toolchain=$(sed -n 's/^channel = "\([^"]*\)"/\1/p' rust/rust-toolchain.toml)
rustup run "$toolchain" cargo test --locked --offline --manifest-path rust/Cargo.toml -p nuvio-core
export NUVIO_JSON_RUST_LIBRARY
NUVIO_JSON_RUST_LIBRARY=$(bash tools/rust-core.sh)
flags=(-O1 -g -Isrc -std=gnu11 -DNV_RUST_CORE=1 -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
libraries=("$NUVIO_JSON_RUST_LIBRARY" -ldl -lpthread -lm -lrt)
cc "${flags[@]}" tests/json_reference.c tests/rust_json_reader.c src/js.c "${libraries[@]}" -o "$json_scratch/parity"
"$json_scratch/parity"
for name in jscadeia js_limites; do
  bash "tests/$name.sh"
  bash "tests/$name.sh" --rust
done

# Neighboring modules retain their existing fixtures and C writer. Network and
# disk dependencies are supplied by the tests; no actual account is accessed.
consumer_flags=(-O1 -g -Isrc -std=gnu11 -DNV_RUST_CORE=1 -Wall -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then consumer_flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
cc "${consumer_flags[@]}" src/profiles.c src/js.c src/jsw.c tests/perfilsel.c "${libraries[@]}" -o "$json_scratch/profiles"
mkdir "$json_scratch/data"
NUVIO_TESTE_DIR="$json_scratch/data" "$json_scratch/profiles"
NUVIO_TESTE_DIR="$json_scratch/data" "$json_scratch/profiles" dois
cc "${consumer_flags[@]}" src/syncprog.c src/progress.c src/js.c src/jsw.c tests/syncprog.c "${libraries[@]}" -o "$json_scratch/sync"
"$json_scratch/sync"
cc "${consumer_flags[@]}" src/contalib.c src/js.c src/vistoep.c tests/contalib.c "${libraries[@]}" -o "$json_scratch/library"
"$json_scratch/library"
cc "${consumer_flags[@]}" src/xtream.c src/js.c tests/xtream.c "${libraries[@]}" -o "$json_scratch/channels"
"$json_scratch/channels"
read -r -a dependency_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
cc "${consumer_flags[@]}" -DNV_LINUX_DESKTOP "${dependency_flags[@]}" \
  -ffunction-sections -fdata-sections '-Wl,--gc-sections' \
  src/stream_parse.c src/js.c src/badges.c tests/stream_parser.c \
  "${libraries[@]}" -o "$json_scratch/streams"
"$json_scratch/streams"
bash tests/addonurl.sh --rust
echo 'Rust JSON account/provider integration: ok'
