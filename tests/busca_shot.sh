#!/bin/bash
# Capturas da Busca para revisao visual. A compilacao exclui novidades1312.c
# porque o pacote de trabalho atual tem esse arquivo em edicao por outro agente;
# os simbolos da tela ficam neutros neste harness e nenhuma fonte e revertida.
set -eu
cd "$(dirname "$0")/.."
IMAGE_TOOL="$(command -v magick || command -v convert)"
saida="${1:-/tmp/nuvio-busca}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-shot-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = "src/main.c" ] && continue
  [ "$source" = "" ] && continue
  sources+=("$source")
done
cc "${sources[@]}" tests/busca_shot.c -x c - -Isrc -o "$tmp/shot" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -O1 -g \
  -DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)   \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz $(pkg-config --libs glesv2 egl) -ldl -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined <<'EOF'
int novidades1312_aberto(void) { return 0; }
int novidades1312_primeira_vez(void) { return 0; }
void novidades1312_atualizar(float dt, unsigned int agora) { (void)dt; (void)agora; }
void novidades1312_desenhar(unsigned int agora) { (void)agora; }
void novidades1312_evento(const void *e) { (void)e; }
EOF
dados="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-busca-dados-XXXXXX")"
NUVIO_DADOS="$dados" "$tmp/shot" "$saida"
# JPEG de 1400 px para olhar: o BMP de 1920x1080 tem 8 MB e nao abre em todo
# visualizador. ImageMagick on Linux.
if command -v "$IMAGE_TOOL" >/dev/null 2>&1; then
  for b in "$saida"-*.bmp; do
    "$IMAGE_TOOL" "$b" -resize "1400x1400>" "${b%.bmp}.jpg" >/dev/null
  done
fi
