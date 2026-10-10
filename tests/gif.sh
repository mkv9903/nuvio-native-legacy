#!/bin/bash
# Offline first-frame decoding, including malformed input under SANITIZE=1.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/gif.c tests/gif.c \
  -Isrc -o /tmp/nuvio-gif-tests -O1 -g \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-gif-tests
