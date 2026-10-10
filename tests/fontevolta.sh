#!/bin/bash
# Regras da fonte guardada para o Retomar. Sem rede: sonda falsa.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontevolta.XXXXXX")
trap 'rm -rf "$dir"' EXIT
# stream_url_serve vem de streams.c, que puxa o resto do app: aqui so a
# assinatura, porque o teste troca a sonda.
cat > "$dir/sonda.c" <<'C'
int stream_url_serve(const char *u, const char *c) { (void)u; (void)c; return 0; }
C
cc -O1 -g -Wall -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  ${SANITIZE:+-fsanitize=address,undefined} \
  src/fontevolta.c "$dir/sonda.c" tests/fontevolta.c -o "$dir/t" -lpthread
"$dir/t"
