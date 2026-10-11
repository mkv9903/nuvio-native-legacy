#!/bin/bash
# Shared setup for the translated reader regressions, sourced by their runners.
json_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-json.XXXXXXXX")
trap 'rm -rf "$json_scratch"' EXIT
json_flags=(-O1 -g -Isrc -std=gnu11 -Wall -Wextra -Werror)
json_libraries=()
case "${1:-}" in
  '') ;;
  --rust)
    json_flags+=(-DNV_RUST_CORE=1)
    json_libraries+=("${NUVIO_JSON_RUST_LIBRARY:-$(bash tools/rust-core.sh)}" -ldl -lpthread -lm -lrt) ;;
  *) echo 'Usage: bash tests/{jscadeia,js_limites}.sh [--rust]' >&2; exit 2 ;;
esac
if [ "$#" -gt 1 ]; then echo 'Expected at most one --rust argument' >&2; exit 2; fi
if [ "${SANITIZE:-0}" = 1 ]; then
  json_flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer)
fi
