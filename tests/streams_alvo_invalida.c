// Teste de regressao: invalidacao de fontes de outro alvo.
// Garante que abrir a folha para um novo alvo nunca exibe fontes do alvo anterior.
#include "streams.h"
#include "ajustes.h"
#include "debrid.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

int main(void) {
  Stream lA[2];
  Stream lB[1];
  char dir[] = "/tmp/nuvio-test-invalida-XXXXXX";
  char caminho[512];
  FILE *f;

  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "fonteAutoLocal 0\nqualidade 0\n");
  fclose(f);
  ajustes_dir(dir);

  memset(lA, 0, sizeof lA);
  snprintf(lA[0].rotulo, sizeof lA[0].rotulo, "Filme A 1080p");
  snprintf(lA[0].url, sizeof lA[0].url, "https://test.invalid/a1.mp4");
  lA[0].altura = 1080;
  lA[0].fileIdx = -1;

  snprintf(lA[1].rotulo, sizeof lA[1].rotulo, "Filme A 720p");
  snprintf(lA[1].url, sizeof lA[1].url, "https://test.invalid/a2.mp4");
  lA[1].altura = 720;
  lA[1].fileIdx = -1;

  // 1. Alvo A: define e popula lista
  stream_definir_alvo("tt0000001");
  stream_definir_lista(lA, 2);
  assert(stream_n() == 2);
  assert(stream_lista_do_alvo("tt0000001") == 1);
  assert(stream_lista_do_alvo("tt0000002") == 0);

  // 2. Abrir folha para o MESMO alvo mantem as fontes
  stream_folha_abrir();
  assert(stream_n() == 2);
  assert(stream_folha_aberta() == 1);
  tecla(SDLK_ESCAPE);
  assert(stream_folha_aberta() == 0);

  // 3. Alvo B selecionado: alvoPedido agora e tt0000002
  stream_definir_alvo("tt0000002");
  assert(stream_lista_do_alvo("tt0000002") == 0);

  // 4. Abrir folha para Alvo B ANTES de novas fontes chegarem:
  // DEVE descartar fontes do Alvo A imediatamente (stream_n == 0)
  stream_folha_abrir();
  assert(stream_n() == 0);
  assert(stream_lista_do_alvo("tt0000001") == 0);
  assert(stream_folha_aberta() == 1);
  tecla(SDLK_ESCAPE);
  assert(stream_folha_aberta() == 0);

  // 5. Novas fontes do Alvo B chegam: entram normalmente
  memset(lB, 0, sizeof lB);
  snprintf(lB[0].rotulo, sizeof lB[0].rotulo, "Filme B 4K");
  snprintf(lB[0].url, sizeof lB[0].url, "https://test.invalid/b1.mp4");
  lB[0].altura = 2160;
  lB[0].fileIdx = -1;
  stream_definir_lista(lB, 1);
  assert(stream_n() == 1);
  assert(stream_lista_do_alvo("tt0000002") == 1);

  // 6. Abrir folha para Alvo B com suas proprias fontes mantem as fontes
  stream_folha_abrir();
  assert(stream_n() == 1);
  assert(stream_folha_aberta() == 1);
  tecla(SDLK_ESCAPE);
  assert(stream_folha_aberta() == 0);

  printf("streams_alvo_invalida: PASS (fontes do alvo anterior descartadas ao abrir folha para novo alvo)\n");
  return 0;
}
