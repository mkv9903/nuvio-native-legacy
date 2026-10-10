#!/bin/bash
# Modo seguro: diario, reversao apos queda, confirmacao e laco de quedas.
#
#   bash tests/seguro.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/safemode.c tests/seguro.c -Isrc \
  -o /tmp/nuvio-seguro-tests -O1 -g -Wall -Wno-deprecated-declarations
/tmp/nuvio-seguro-tests
