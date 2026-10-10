#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Focused syntax gate for the three surfaces in this change; no link or full app build.
cc -fsyntax-only src/guia.c src/streams.c  \
  -Isrc -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)  \
  -Wno-deprecated-declarations -Wno-macro-redefined

# Both Guide drawers use the configured Glass leaf and retain an opaque path.
[ "$(rg -c 'materialPainelGuia\(p,' src/guia.c)" -eq 2 ]
rg -q 'if \(ajustes_vidro\(\)\) gfx_vidro_folha\(p, raio, a\)' src/guia.c
rg -q 'else gfx_cor\(p, raio, 0\.071f, 0\.075f, 0\.086f, 0\.98f \* a\)' src/guia.c

# The Sources sheet uses adjustable opacity/Frost in Glass mode and the shared
# opaque material when Glass is off.
rg -q 'gfx_vidro_folha\(corpo,FOLHA_RAIO_IL/corpo.h,anim\)' src/streams.c
rg -q 'else plrui_material\(corpo,FOLHA_RAIO_IL,0,anim\)' src/streams.c

echo 'glass surface contract: PASS'
