#!/bin/bash
# Retained C profile cycles/credentials; Home-order cases have moved to Rust.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O1 -g -Isrc -pthread -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
       -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="${TMPDIR:-/tmp}/nuvio-syncordem-tests"
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/catalog_order_adapter.c src/contacache.c src/js.c src/jsw.c \
  tests/syncordem.c -o "$bin"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-syncordem.XXXXXXXX")"
trap 'rm -rf "$dir"' EXIT
export NV_T_DIR="$dir/space.mkv.nuvio.rusttest"
mkdir -p "$NV_T_DIR"

sessao() {
  local saida
  if ! saida=$("$bin" "$@" 2>&1); then echo "$saida"; echo "syncordem.sh: FALHOU"; exit 1; fi
  echo "$saida" | grep -E '^(--|  )|restaurada|guardada|ordem da conta' || true
  SAIDA="$saida"
}

# Profile choice after, during and just before worker publication.
sessao 2
sessao 2 segura
sessao 1
sessao 2 segura
sessao 1 tarde
# 7. No perfil 1, volta ao 2 com o ciclo do 1 no ar: o ciclo do 1 inteiro e
#    descartado, e o do 2 roda.
sessao troca
echo "$SAIDA" | grep -qF '[sync] ciclo do perfil 1 descartado' \
  || { echo "FALHOU: sessao 7 nao descartou o ciclo do perfil 1"; exit 1; }
# 8. Servidor que nao aceita credencial trakt/simkl (400 "Unsupported provider
#    credential"): uma pergunta por provedor na sessao, nao uma por ciclo.
sessao credencial
echo "$SAIDA" | grep -qF 'nao tento de novo por 7 dias' \
  || { echo "FALHOU: sessao 8 nao anotou a recusa"; exit 1; }
sessao credencial-reabrir
echo "$SAIDA" | grep -qF 'recusa lembrada do disco' \
  || { echo "FALHOU: recusa nao sobreviveu ao arranque"; exit 1; }
# Addon edits, offline journals and acknowledgements now run in Rust tests.
echo "$SAIDA" | grep -q 'FALHOU' && exit 1
echo "syncordem.sh: ok"
