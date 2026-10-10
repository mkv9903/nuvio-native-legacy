#!/bin/bash
# "Termina as HH:MM" do player cabe em todos os idiomas (issue #213).
set -eu
cd "$(dirname "$0")/.."
cc tests/relogiofim.c src/idioma.c -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) \
    -o /tmp/nuvio-relogiofim-tests \
   -Wall -Wno-macro-redefined -Wno-deprecated-declarations
/tmp/nuvio-relogiofim-tests
# Desde o Glass UI (03/10) a frase mora so na pilula da ilha do player
# (plrilha.c; a pausa diz "Pausado" nela): o helper e o buffer de tamanho certo.
grep -q 'relogio_fim_ilha(fim, nf, t, falta)' src/plrilha.c
grep -q 'fim\[RELOGIO_FIM_MAX\]' src/plrilha.c
echo "relogiofim: a pilula do player usa o helper"
