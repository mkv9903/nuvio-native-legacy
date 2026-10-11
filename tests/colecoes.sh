#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
for platform in desktop webos; do
  flags=()
  if [ "$platform" = webos ]; then flags+=(-DNV_WEBOS); fi
  cc "${flags[@]}" -O1 -g -Wall -Wextra -Isrc -Wno-misleading-indentation src/colecoes.c src/net_url.c src/js.c tests/colecoes.c -o "/tmp/nuvio-colecoes-tests-$platform"
  "/tmp/nuvio-colecoes-tests-$platform"
done
