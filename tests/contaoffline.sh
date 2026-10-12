#!/bin/bash
# #215: o app sobrevive ao servidor da conta fora do ar (504/429/sem resposta).
# Ver o cabecalho de tests/contaoffline.c. Sem rede.
#
#   bash tests/contaoffline.sh
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin=/tmp/nuvio-contaoffline-tests
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/catalog_order_adapter.c src/catalog_order_cache_adapter.c src/contacache.c \
  src/js.c src/jsw.c tests/contaoffline.c -o "$bin"
dir="$(mktemp -d)"
trap 'rm -rf "$dir"' EXIT
export NV_T_DIR="$dir"

sessao() {
  local saida
  if ! saida=$("$bin" "$@" 2>&1); then echo "$saida"; echo "contaoffline.sh: FALHOU"; exit 1; fi
  echo "$saida" | grep -E '^(--|  )|\[sync\] servidor da conta' || true
  SAIDA="$saida"
}
# These checks cover retained C capabilities only. Rust owns addon outage,
# same-scope cache, large URLs, logout and edit-journal coverage.
sessao 504
sessao 200
sessao 504
sessao 429
sessao 0
sessao parcial
sessao 502 - volta
sessao 504 conta-b
sessao 200 - sair
sessao 504
echo "contaoffline.sh: ok"
