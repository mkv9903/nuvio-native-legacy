#!/bin/bash
# Leitores privados EBML/lacing; sem sockets, SDL ou arquivos de midia.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-mkvass-limites.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
flags=(-ffunction-sections -fdata-sections)
flags+=(-Wl,--gc-sections -pthread -lm)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc tests/mkvass_limites.c -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations \
  "${flags[@]}" -o "$DIR/test"
"$DIR/test"
