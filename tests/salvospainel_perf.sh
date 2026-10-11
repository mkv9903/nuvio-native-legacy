#!/bin/bash
# Medida por quadro do painel de Salvos (tecla AZUL) com a home por baixo.
# Nao entra na suite: precisa de janela GL, e o numero e para comparar, nao
# para passar/falhar. A trava que passa/falha e tests/salvospainel.sh.
#
#   bash tests/salvospainel_perf.sh            # esta arvore
#   bash tests/salvospainel_perf.sh /outra/arvore   # ex.: worktree da v1.4.5
#
# PERF_SEM_HOME=1 mede so o painel.
set -eu
aqui="$(cd "$(dirname "$0")/.." && pwd)"
raiz="${1:-$aqui}"
cd "$raiz"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-spperf-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
flags=()
if grep -q spainel_n_reconstrucoes src/salvospainel.c; then flags+=(-DSP_CONTADOR); fi
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" "$aqui/tests/salvospainel_perf.c" -Isrc -o "$tmp/perf" \
  ${flags[@]+"${flags[@]}"} -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O2 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/perf" 2>&1 | grep -vE "^\[(tex|arte|cat|desc|home|salvos|txt)\]"
