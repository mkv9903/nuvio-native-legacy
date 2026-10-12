#!/bin/bash
# Custo do ciclo de sync (colecoes da conta) no fio principal. Ver
# tests/sync_aplicar_perf.c. Medida, nao passa/falha.
set -eu
cd "$(dirname "$0")/.."
source tools/test-support.sh
flags=(-O2 -g -std=gnu11 -DFIL_TESTE -Isrc -pthread -Wno-misleading-indentation)
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-sync-aplicar-perf.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sync-aplicar-perf-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/colfileiras.c src/colecoes.c src/fileiras.c src/catalog_order_adapter.c src/js.c src/net_url.c tests/sync_aplicar_perf.c -o "$bin"
NV_T_DIR="$dir" "$bin" 2>&1 | grep -vE "^\[(colecoes|collections|fileiras)\]"
