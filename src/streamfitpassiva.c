#include "streamfitpassiva.h"
#include "streamfit.h"
#include "redemarca.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static uint64_t permitida;

void streamfitpassiva_permitir(uint64_t geracao) {
  pthread_mutex_lock(&trava);
  permitida = geracao;
  pthread_mutex_unlock(&trava);
}

int streamfitpassiva_receber(uint64_t rede, uint64_t geracao, const char *origem,
                             const int *kbps, int n, uint64_t fimMs) {
  int aceitos = 0;
  const char *motivo = "accepted";
  // Only an authority: a path or query from the producer is refused outright,
  // so a resolver URL can never be stored as if it were the media host.
  const char *autoridade = !origem ? NULL : !strncmp(origem, "https://", 8) ? origem + 8 :
                           !strncmp(origem, "http://", 7) ? origem + 7 : NULL;
  if (!autoridade || !*autoridade || strpbrk(autoridade, "/?#@")) motivo = "origin";
  else {
    // Held across the engine call: a generation switch on the UI thread cannot
    // interleave between the check and the store (lock order: this -> engine).
    pthread_mutex_lock(&trava);
    if (!geracao || geracao != permitida) motivo = "stale_generation";
    else if (!rede || rede != redemarca_atual()) motivo = "network";
    else if (!(aceitos = streamfit_passiva(rede, origem, kbps, n, fimMs))) motivo = "rejected";
    pthread_mutex_unlock(&trava);
  }
  /* English, counts only: no host, URL, network id or title. */
  printf("[stream_fit] passive %s intervals=%d accepted=%d\n", motivo, n, aceitos);
  fflush(stdout);
  return aceitos;
}
