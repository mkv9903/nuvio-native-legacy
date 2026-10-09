#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-rede-buffer.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -O1 -g -Wall -Wextra -fsanitize=address,undefined -Isrc \
  tests/rede_buffer.c src/net_url.c -lpthread -o "$DIR/teste"
"$DIR/teste"
