#!/bin/bash
# A arte escolhida a mao (#142): tabela, disco por perfil, logout e a
# precedencia sobre a politica automatica de artehero.c. Sem SDL e sem rede.
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o proprio teste se recusa a
# rodar se dados_dir() nao for ela.
#
#   bash tests/arteescolha.sh
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-arteesc.XXXXXX)
export NUVIO_DADOS
obj=$(mktemp -d /tmp/nuvio-arteesc-obj.XXXXXX)
trap 'rm -rf "$NUVIO_DADOS" "$obj"' EXIT

flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
comum=(-Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  -O1 -g
       -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined)
for alvo in lg; do
  def=()
  cc ${flags[@]+"${flags[@]}"} ${def[@]+"${def[@]}"} "${comum[@]}" -c src/artehero.c -o "$obj/hero-$alvo.o"
  cc ${flags[@]+"${flags[@]}"} ${def[@]+"${def[@]}"} "${comum[@]}" -c tests/arteescolha.c -o "$obj/teste-$alvo.o"
  cc ${flags[@]+"${flags[@]}"} "${comum[@]}" src/arteescolha.c src/data.c \
    "$obj/hero-$alvo.o" "$obj/teste-$alvo.o" -o "$obj/arteesc-$alvo"
  find "$NUVIO_DADOS" -mindepth 1 -delete
  echo "== $alvo"
  "$obj/arteesc-$alvo"
done
