#!/bin/bash
# Frases (Wikiquote) e ficha de producao (Wikidata): leitura da resposta SPARQL,
# limpeza de wikitexto, extracao das falas nos dois formatos, e o cache — com o
# CACHE NEGATIVO, que e o que segura o custo dos titulos que nao tem nada.
#
#   bash tests/seriefrases.sh
#
# Compila tudo MENOS src/seriefrases.c, que o teste inclui.
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o proprio teste se recusa a
# rodar se dados_dir() nao for ela.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-seriefrases-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/seriefrases.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/seriefrases.c -Isrc -o /tmp/nuvio-seriefrases \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-seriefrases
