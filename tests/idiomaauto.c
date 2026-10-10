// IDIOMA AUTOMATICO: o mapa codigo -> IDIOMA_* e a ordem de precedencia.
// Funcao pura (src/idiomaauto.h), sem SDL, sem disco.
#include "../src/idiomaauto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define M(cod, esperado) do { \
  int r_ = idiomaauto_mapear(cod); \
  if (r_ != (esperado)) { printf("FALHOU mapear(\"%s\") = %d, esperava %d\n", \
    (cod) ? (cod) : "(nulo)", r_, (esperado)); return 1; } } while (0)

#define R(tmdb, leg, sis, idioma, origem) do { \
  int f_ = -1, r_ = idiomaauto_resolver(tmdb, leg, sis, &f_); \
  if (r_ != (idioma) || f_ != (origem)) { \
    printf("FALHOU resolver(%s, %s, %s) = %d/%d, esperava %d/%d\n", \
      (tmdb) ? (tmdb) : "NULL", (leg) ? (leg) : "NULL", (sis) ? (sis) : "NULL", \
      r_, f_, (idioma), (origem)); return 1; } } while (0)

int main(void) {
  // ingles e variantes
  M("en-US", IDIOMA_EN);  M("en-GB", IDIOMA_EN);  M("en", IDIOMA_EN);
  M("en_US.UTF-8", IDIOMA_EN); M("eng", IDIOMA_EN);

  // codigos desconhecidos / nao-ingles
  M("pt-BR", -1); M("fr", -1); M("de", -1); M("es", -1);
  M(NULL, -1); M("", -1); M("none", -1); M("off", -1);

  // precedencia: tmdb > legenda > sistema > padrao
  R("en-US", "es", "fr", IDIOMA_EN, IDA_TMDB);
  R(NULL, "eng", "fr", IDIOMA_EN, IDA_LEGENDA);
  R(NULL, NULL, "en", IDIOMA_EN, IDA_SISTEMA);
  R(NULL, NULL, NULL, IDIOMA_EN, IDA_PADRAO);
  R("pt-BR", "de", "ru", IDIOMA_EN, IDA_PADRAO);

  assert(!strcmp(idiomaauto_codigo(IDIOMA_EN), "en"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_TMDB), "tmdb_language"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_LEGENDA), "legenda"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_SISTEMA), "sistema"));
  assert(!strcmp(idiomaauto_fonte_nome(IDA_PADRAO), "padrao"));

  { int i;
    for (i = 0; i < IDIOMA_N; i++)
      assert(idiomaauto_mapear(idiomaauto_codigo(i)) == i);
  }
  puts("idiomaauto: ok");
  return 0;
}
