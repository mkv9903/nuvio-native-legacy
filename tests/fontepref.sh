#!/bin/bash
# A fonte lembrada: assinatura de idioma, casamento entre episodios, disco e
# separacao por perfil. Issues #56 e #57.
#
#   bash tests/fontepref.sh
#
# Compila tudo MENOS src/fontepref.c, que o teste inclui — normalizar(),
# tokenEm() e a tabela de termos sao estaticas, e o teste tambem precisa forcar
# um arranque frio (mesma receita de tests/atualizacao.sh e tests/recomenda.sh).
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o proprio teste se recusa a
# rodar se dados_dir() nao for ela. Sem esta linha ele escreveria dentro do
# ~/.nuvio de quem o executa, por cima das preferencias, da lista de salvos e da
# sessao de verdade.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-fontepref.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados do teste em $NUVIO_DADOS"

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/fontepref.c) continue;; esac
  sources+=("$source")
done

flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi

cc ${flags[@]+"${flags[@]}"} "${sources[@]}" tests/fontepref.c -Isrc -o /tmp/nuvio-fontepref \
  -O1 -g -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
   -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fontepref "$@"
