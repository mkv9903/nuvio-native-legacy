// Exercise the real LG event queue with mock controls excluded from the build.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/sistexto.h"
#include "../src/entrada_texto.h"

const char *desc_tmdb_idioma(void) { return "en-US"; }

static int ler(char *value, size_t size) {
  SDL_Event event;
  texto_sistema_quadro();
  while (SDL_PollEvent(&event)) st_evento(&event);
  return st_ler(ST_BUSCA, value, size);
}

int main(void) {
  char value[300];
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  assert(st_ime_disponivel() && !st_voz_disponivel());
  assert(!st_abre_sozinho());
  assert(st_ime_abrir(ST_BUSCA, "", 256));
  assert(st_estado() == ST_DIGITANDO);
  texto_sistema_ponte_valor("matrix ç");
  assert(ler(value, sizeof value) == ST_TEXTO);
  assert(!strcmp(value, "matrix ç"));
  texto_sistema_ponte_valor("matrix final");
  texto_sistema_ponte_fim(1);
  assert(ler(value, sizeof value) == ST_FIM);
  assert(!strcmp(value, "matrix final") && st_estado() == ST_PARADO);
  // Cancelling and reopening must not apply text from the previous field.
  assert(st_ime_abrir(ST_BUSCA, "", 256));
  texto_sistema_ponte_fim(0);
  assert(ler(value, sizeof value) == ST_CANCELOU);
  assert(st_ime_abrir(ST_BUSCA, "new field", 256));
  assert(ler(value, sizeof value) == ST_NADA);
  st_fechar(ST_BUSCA);
  SDL_Quit();
  puts("LG system text: PASS (text, confirm, cancel, reopen; no mock mode)");
  return 0;
}
