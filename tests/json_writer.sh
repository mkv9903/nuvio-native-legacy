#!/bin/bash
# Same focused byte/ownership/allocation regressions on either implementation.
set -euo pipefail
cd "$(dirname "$0")/.."
json_writer_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-json-writer.XXXXXXXX")
trap 'rm -rf "$json_writer_scratch"' EXIT
flags=(-O1 -g -Isrc -std=gnu11 -Wall -Wextra -Werror)
libraries=()
case "${1:-}" in
  '') ;;
  --rust)
    flags+=(-DNV_RUST_CORE=1)
    libraries+=("${NUVIO_JSON_RUST_LIBRARY:-$(bash tools/rust-core.sh)}" -ldl -lpthread -lm -lrt) ;;
  *) echo 'Usage: bash tests/json_writer.sh [--rust]' >&2; exit 2 ;;
esac
if [ "$#" -gt 1 ]; then echo 'Expected at most one --rust argument' >&2; exit 2; fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/jsw.c tests/json_writer_reference.c tests/json_writer.c \
  '-Wl,--wrap=realloc' '-Wl,--wrap=free' "${libraries[@]}" -o "$json_writer_scratch/test"
"$json_writer_scratch/test"
