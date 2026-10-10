#!/bin/bash
# src/net_url.c anda junto com src/net.c: rede.c imprime URL redigida no log e
# a redacao mora naquele arquivo (ver o cabecalho dele). Quem compila por glob
# de src/*.c src/dts/*.c nao precisa pensar nisso; quem lista fontes a mao, sim.
set -eu
cd "$(dirname "$0")/.."
cc -Isrc tests/mkv_caps.c src/net.c src/net_url.c -o /tmp/nuvio-mkv-caps-tests \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf) -Wno-deprecated-declarations 2>/dev/null || \
cc -Isrc tests/mkv_caps.c src/net.c src/net_url.c -o /tmp/nuvio-mkv-caps-tests -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)
/tmp/nuvio-mkv-caps-tests
