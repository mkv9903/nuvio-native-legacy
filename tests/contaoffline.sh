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
precisa() { echo "$SAIDA" | grep -qF "$1" || { echo "FALHOU: faltou no log: $1"; exit 1; }; }

# 1. Primeira abertura com o servidor fora: nada salvo.
sessao 504
precisa '[sync] servidor da conta indisponivel (HTTP 504) e nenhuma copia de addons neste aparelho'
# 2. Servidor bom: a resposta vira copia.
sessao 200
# 3. Servidor fora (o log da TCL): os addons voltam da copia.
sessao 504
precisa '[sync] servidor da conta indisponivel (HTTP 504): usando a copia de '
sessao 429
precisa '[sync] servidor da conta indisponivel (HTTP 429): usando a copia de '
sessao 0
precisa '[sync] servidor da conta indisponivel (sem resposta): usando a copia de '
# 4. So as RPCs caem (429), a tabela de addons responde: cada superficie
#    cai para a sua copia.
sessao parcial
precisa '[sync] servidor da conta indisponivel (HTTP 429): usando a copia de '
# 5. Fora e depois volta.
sessao 502 - volta
# 6. A copia e da conta: outra pessoa nesta TV nao a usa; logout apaga.
sessao 504 conta-b
sessao 200 - sair
sessao 504
precisa 'nenhuma copia de addons'
# 7. #201: URL de addon longa chega inteira; a que passa do limite e recusada
#    com o nome e o tamanho no log, e a URL em si nunca aparece nele.
sessao longa
precisa '[addons] Grande: URL de 2600 caracteres nao cabe (maximo 2047): addon ignorado'
precisa '[sync] edicao de addons nao enviada: 1 addon(s) da conta nao cabem nesta TV'
if echo "$SAIDA" | grep -q 'SEGREDO'; then echo "FALHOU: URL de addon no log"; exit 1; fi
echo "contaoffline.sh: ok"
