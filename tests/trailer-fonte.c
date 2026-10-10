// Native trailer selection: priority, waiting, failure and explicit sources.
#include "../src/trailerfonte.h"
#include <stdio.h>
#include <string.h>

// trailerfonte.c le o ajuste por esta funcao; aqui ela e um numero do teste.
static int ajusteTeste;
int ajustes_trailer_fonte(void) { return ajusteTeste; }

static int falhas;
static void confere(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok " : "FALHOU", nome);
  if (!ok) falhas++;
}

// Todas as fontes com trailer e ja respondidas.
static TrailerCandidatos todas(void) {
  TrailerCandidatos c;
  memset(&c, 0, sizeof c);
  c.apple = "https://apple/v.m3u8"; c.appleRespondeu = 1;
  c.imdb = "https://imdb/v.mp4";    c.imdbRespondeu = 1;
  return c;
}

static int escolhe(int aj, const TrailerCandidatos *c, const char **u) {
  int q = -1; const char *x = NULL;
  TrailerDecisao d = trailerfonte_escolher(aj, c, &x, &q);
  if (u) *u = x;
  return d == TRF_ABRE ? q : d == TRF_ESPERA ? -1 : 0;
}

int main(void) {
  int o[2], n;
  TrailerCandidatos c;
  const char *u;
  n = trailerfonte_ordem(TRF_AUTO, o);
  confere("native automatic: Apple -> IMDb", n == 2 && o[0] == TRF_APPLE && o[1] == TRF_IMDB);
  n = trailerfonte_ordem(99, o);
  confere("unknown stored value reads as automatic", n == 2 && o[0] == TRF_APPLE);
  {
    char nome[120];
    c = todas();
    snprintf(nome, sizeof nome, "%s automatico: Apple primeiro com todas", "native");
    confere(nome, escolhe(TRF_AUTO, &c, &u) == TRF_APPLE && !strcmp(u, c.apple));

    // A Apple ainda nao respondeu: a proxima NAO ganha por chegar antes.
    c = todas(); c.apple = NULL; c.appleRespondeu = 0;
    snprintf(nome, sizeof nome, "%s automatico: espera a Apple responder", "native");
    confere(nome, escolhe(TRF_AUTO, &c, NULL) == -1);

    // A Apple respondeu sem trailer: passa a vez a proxima da plataforma.
    c.appleRespondeu = 1;
    snprintf(nome, sizeof nome, "%s automatico: Apple vazia cede a vez", "native");
    confere(nome, escolhe(TRF_AUTO, &c, &u) == TRF_IMDB);

    // A Apple deu erro no elemento: mesma coisa, sem reabrir a mesma URL.
    c = todas(); c.appleFalhou = 1;
    snprintf(nome, sizeof nome, "%s automatico: Apple com erro cede a vez", "native");
    confere(nome, escolhe(TRF_AUTO, &c, &u) == TRF_IMDB);
    snprintf(nome, sizeof nome, "%s automatico: depois da Apple vem a proxima", "native");
    confere(nome, trailerfonte_depois(TRF_AUTO, TRF_APPLE) == TRF_IMDB);

    // Ninguem tem: sem trailer (e nao espera para sempre).
    memset(&c, 0, sizeof c);
    c.appleRespondeu = c.imdbRespondeu = 1;
    snprintf(nome, sizeof nome, "%s automatico: ninguem tem, sem trailer", "native");
    confere(nome, escolhe(TRF_AUTO, &c, NULL) == 0);

    // --- Fonte fixa: SO ela
    c = todas();
    snprintf(nome, sizeof nome, "%s Apple fixa: abre Apple", "native");
    confere(nome, escolhe(TRF_APPLE, &c, &u) == TRF_APPLE);
    c.apple = NULL;
    snprintf(nome, sizeof nome, "%s Apple fixa sem Apple: sem trailer, nada de reserva", "native");
    confere(nome, escolhe(TRF_APPLE, &c, NULL) == 0);
    c = todas(); c.appleFalhou = 1;
    snprintf(nome, sizeof nome, "%s Apple fixa com erro: sem proxima", "native");
    confere(nome, escolhe(TRF_APPLE, &c, NULL) == 0 && trailerfonte_depois(TRF_APPLE, TRF_APPLE) == 0);

    c = todas();
    snprintf(nome, sizeof nome, "%s IMDb fixo: %s", "native", "abre IMDb, pula a Apple");
    confere(nome, escolhe(TRF_IMDB, &c, &u) == TRF_IMDB);
    c.appleRespondeu = 0; c.apple = NULL;
    snprintf(nome, sizeof nome, "%s IMDb fixo nao espera a Apple", "native");
    confere(nome, escolhe(TRF_IMDB, &c, NULL) == TRF_IMDB);

    c = todas();
    snprintf(nome, sizeof nome, "%s YouTube fixo: %s", "native", "nao toca nesta TV");
    confere(nome, escolhe(TRF_YOUTUBE, &c, &u) == 0);
    snprintf(nome, sizeof nome, "%s YouTube fixo ainda sem lista: %s", "native", "sem trailer");
    confere(nome, escolhe(TRF_YOUTUBE, &c, NULL) == 0);

    for (n = TRF_APPLE; n <= TRF_YOUTUBE; n++) {
      int k = trailerfonte_ordem(n, o);
      snprintf(nome, sizeof nome, "%s ajuste %d: no maximo uma fonte, e a dele", "native", n);
      confere(nome, k <= 1 && (k == 0 || o[0] == n));
    }
  }

  ajusteTeste = TRF_IMDB;
  confere("saved source", trailerfonte_ajuste() == TRF_IMDB);
  return falhas ? 1 : 0;
}
