#include "trailerfonte.h"
#include "ajustes.h"
#include <stddef.h>

int trailerfonte_ordem(int ajuste, int ordem[2]) {
  if (ajuste == TRF_APPLE || ajuste == TRF_IMDB) {
    ordem[0] = ajuste;
    return 1;
  }
  /* A legacy embedded YouTube choice has no native player. */
  if (ajuste == TRF_YOUTUBE) return 0;
  ordem[0] = TRF_APPLE;
  ordem[1] = TRF_IMDB;
  return 2;
}

static TrailerDecisao escolherNaOrdem(const int *ordem, int n, const TrailerCandidatos *c,
                                      const char **url, int *qual) {
  int i;
  if (url) *url = NULL;
  if (qual) *qual = 0;
  if (!c) return TRF_NENHUMA;
  for (i = 0; i < n; i++) {
    const char *u = NULL;
    int respondeu = 1;
    switch (ordem[i]) {
      case TRF_APPLE:
        if (c->appleFalhou) continue;   // ja deu erro nesta tentativa: cede
        u = c->apple; respondeu = c->appleRespondeu; break;
      case TRF_IMDB:    u = c->imdb;    respondeu = c->imdbRespondeu; break;
    }
    if (u && u[0]) {
      if (url) *url = u;
      if (qual) *qual = ordem[i];
      return TRF_ABRE;
    }
    if (!respondeu) return TRF_ESPERA;
  }
  return TRF_NENHUMA;
}

TrailerDecisao trailerfonte_escolher(int ajuste, const TrailerCandidatos *c,
                                     const char **url, int *qual) {
  int ordem[2], n = trailerfonte_ordem(ajuste, ordem);
  return escolherNaOrdem(ordem, n, c, url, qual);
}



int trailerfonte_depois(int ajuste, int qual) {
  int ordem[2], n = trailerfonte_ordem(ajuste, ordem), i;
  for (i = 0; i + 1 < n; i++)
    if (ordem[i] == qual) return ordem[i + 1];
  return 0;
}

const char *trailerfonte_nome(int qual) {
  switch (qual) {
    case TRF_APPLE:   return "apple";
    case TRF_IMDB:    return "imdb";
    case TRF_YOUTUBE: return "youtube";
    default:          return "-";
  }
}


int trailerfonte_ajuste(void) { return ajustes_trailer_fonte(); }
