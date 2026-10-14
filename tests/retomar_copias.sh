#!/bin/bash
# Issue #208: o "Retomar" vale em toda copia do titulo, nao so na do CW.
#
#   SANITIZE=1 bash tests/retomar_copias.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c tests/retomar_copias.c \
  -Isrc -o /tmp/nuvio-retomar-copias-tests -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-retomar-copias-tests
