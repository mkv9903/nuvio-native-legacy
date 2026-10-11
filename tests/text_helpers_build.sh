#!/bin/bash
# Shared compiler setup, sourced by the three independently runnable tests.
text_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-text.XXXXXXXX")
trap 'rm -rf "$text_scratch"' EXIT
text_flags=(-O1 -g -Isrc -std=c11 -Wall -Wextra -Werror)
text_libraries=()
case "${1:-}" in
  '') ;;
  --rust)
    text_flags+=(-DNV_RUST_CORE=1)
    text_libraries+=("${NUVIO_TEXT_RUST_LIBRARY:-$(bash tools/rust-core.sh)}" -ldl -lpthread -lm -lrt) ;;
  *) echo 'Usage: bash tests/{dobra,limpa,buscanorm}.sh [--rust]' >&2; exit 2 ;;
esac
if [ "${SANITIZE:-0}" = 1 ]; then
  text_flags+=('-fsanitize=address,undefined' -fno-omit-frame-pointer)
fi
