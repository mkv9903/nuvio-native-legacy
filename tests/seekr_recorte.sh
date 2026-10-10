#!/bin/bash
# Seekr: recorte do quadro dentro da folha, sem rede. Ver tests/seekr_recorte.c.
set -eu
cd "$(dirname "$0")/.."
# Sem SANITIZE: a libSDL2 do brew abre um alerta modal no arranque quando o
# binario tem ASan (medido: dllinit -> error_dialog), e o teste fica parado.
cc src/seekrquota.c src/data.c src/seekrvtt.c src/js.c src/jpegrapido.c tests/seekr_recorte.c \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image $(pkg-config --libs glesv2 egl) -ldl -pthread -lm -o /tmp/nuvio-seekr-recorte -O1 -g \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-seekr-recorte
