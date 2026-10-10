#ifndef NV_TRAILERFONTE_H
#define NV_TRAILERFONTE_H

/* Values are persisted as trailerFonteLocal; do not renumber them. */
enum { TRF_AUTO = 0, TRF_APPLE = 1, TRF_IMDB = 2, TRF_YOUTUBE = 3 };

typedef struct {
  const char *apple;
  int appleRespondeu, appleFalhou;
  const char *imdb;
  int imdbRespondeu;
} TrailerCandidatos;

typedef enum { TRF_ESPERA = 0, TRF_ABRE = 1, TRF_NENHUMA = 2 } TrailerDecisao;

/* Apple first, then IMDb. An explicit source never falls back. */
int trailerfonte_ordem(int ajuste, int ordem[2]);
TrailerDecisao trailerfonte_escolher(int ajuste, const TrailerCandidatos *c,
                                    const char **url, int *qual);
int trailerfonte_depois(int ajuste, int qual);
const char *trailerfonte_nome(int qual);
int trailerfonte_ajuste(void);

#endif
