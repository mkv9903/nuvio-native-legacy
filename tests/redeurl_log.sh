#!/bin/bash
# rede_url_log: config/chave de provedor de poster/meta nunca vai para o log.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-redeurl-log.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -Wall -Wextra -Werror -O1 -g tests/redeurl_log.c src/net_url.c -o "$DIR/t"
"$DIR/t"
