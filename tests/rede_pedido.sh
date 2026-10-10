#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-pedido.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -addext subjectAltName=DNS:localhost -keyout "$tmp/key.pem" \
  -out "$tmp/cert.pem" > "$tmp/cert-generation.log" 2>&1
python3 tests/rede_pedido_server.py "$tmp/ports" "$tmp/cert.pem" "$tmp/key.pem" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/ports" ] && break; sleep 0.1; done
test -s "$tmp/ports"
base=$(sed -n '1p' "$tmp/ports")
tlsbase=$(sed -n '2p' "$tmp/ports")
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread)
flags+=(-ldl)
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${NV_TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi

cc "${flags[@]}" tests/rede_pedido.c src/net.c src/net_url.c -o "$tmp/teste"
"$tmp/teste" "$base" "$tlsbase" "$tmp/cert.pem"
cc "${flags[@]}" tests/rede_pedido_buffer.c src/net_url.c -o "$tmp/buffer"
"$tmp/buffer"
