// idioma_decimal_texto: ponto decimal em ingles ("8.5", "24.1 mi").
#include "idiomacod.h"
#include <stdio.h>
#include <string.h>

static int falhas;
static void caso(int idioma, const char *entrada, const char *esperado) {
  char b[128];
  snprintf(b, sizeof b, "%s", entrada);
  idioma_decimal_texto(b, idioma);
  if (strcmp(b, esperado)) { printf("FALHOU: '%s' -> '%s' (esperado '%s')\n", entrada, b, esperado); falhas++; }
}

int main(void) {
  caso(IDIOMA_EN, "Melhor: T1E2  ·  8.5", "Melhor: T1E2  ·  8.5");
  caso(IDIOMA_EN, "24.1 mi", "24.1 mi");
  caso(IDIOMA_EN, "E1. Piloto", "E1. Piloto");
  caso(IDIOMA_EN, ".5 e 5.", ".5 e 5.");
  caso(IDIOMA_EN, "", "");
  idioma_decimal_texto(NULL, IDIOMA_EN);
  if (!falhas) printf("idioma_decimal: ok\n");
  return falhas != 0;
}
