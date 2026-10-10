// "Termina as HH:MM" no player (issue #213: "Ends at 13:45").
#include "relogiofim.h"
#include "idiomacod.h"
#include <stdlib.h>
#include <string.h>

static int lg = IDIOMA_EN;
int ajustes_idioma(void) { return lg; }
int ajustes_idioma_ingles(void) { return 1; }

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

int main(void) {
  char fim[RELOGIO_FIM_MAX];
  const char *fmt;
  // 2026-10-02 10:10:00 UTC; o filme acaba em 3h35 -> 13:45.
  time_t agora = 1790935800;
  int i;
  setenv("TZ", "UTC", 1);
  tzset();

  // INGLES
  lg = IDIOMA_EN;
  relogio_fim(fim, sizeof fim, agora, 3 * 3600 + 35 * 60);
  CONFERE(!strcmp(fim, "Ends at 13:45"), "en: \"%s\"", fim);
  relogio_fim(fim, sizeof fim, agora, -5);
  CONFERE(!strcmp(fim, "Ends at 10:10"), "en, falta negativa: \"%s\"", fim);

  // Hora de 24 h e de 12 h
  for (i = 0; i < IDIOMA_N; i++) {
    const char *horas[] = { "23:59", "11:59 PM" };
    int h;
    lg = i;
    fmt = i18n("Termina \xc3\xa0" "s %s");
    for (h = 0; h < 2; h++) {
      int n = snprintf(fim, sizeof fim, fmt, horas[h]);
      CONFERE(n < (int)sizeof fim, "idioma %d (%s) nao cabe: %d bytes", i, idioma_iso(i), n);
      CONFERE(strstr(fim, horas[h]) != NULL, "idioma %d: hora cortada \"%s\"", i, fim);
    }
    relogio_fim(fim, sizeof fim, agora, 3 * 3600 + 35 * 60);
    CONFERE(strstr(fim, "13:45") != NULL, "idioma %d: \"%s\"", i, fim);
  }

  if (falhas) { printf("relogiofim: %d falha(s)\n", falhas); return 1; }
  printf("relogiofim: ok\n");
  return 0;
}
