#!/bin/bash
# Renovacao do token com varios fios (#203). Ver tests/sessao_corrida.c. Sem rede.
#   bash tests/sessao_corrida.sh                  (sem sanitizador: so os asserts)
#   SANITIZE=thread bash tests/sessao_corrida.sh  (TSAN)
#   SANITIZE=1 bash tests/sessao_corrida.sh       (ASAN+UBSAN)
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -Wall -Wno-deprecated-declarations)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
run_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sessao-corrida.XXXXXX")
trap 'rm -rf "$run_dir"' EXIT
bin="$run_dir/test"
cc "${flags[@]}" "${SESSAO_C:-src/session.c}" src/js.c src/jsw.c tests/sessao_corrida.c -o "$bin"
falhou=0
for caso in rpc401 vencido r504 leitores; do
  dir="$(mktemp -d "$run_dir/case.XXXXXX")"
  if ! NV_T_DIR="$dir" "$bin" "$caso"; then falhou=1; echo "sessao_corrida.sh: FALHOU ($caso)"; fi
  rm -rf "$dir"
done
[ $falhou = 0 ] || exit 1
echo "sessao_corrida.sh: ok"
