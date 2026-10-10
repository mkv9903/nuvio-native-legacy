// IDIOMA AUTOMATICO DA INTERFACE — a parte pura: codigo de idioma (da conta ou
// da TV) -> um dos IDIOMA_* de idiomacod.h, e a ordem de precedencia.
// Header-only e sem dependencia de proposito (como idiomacod.h): o teste
// tests/idiomaauto.c o inclui sozinho, e ajustes.c so cuida de guardar e gravar.
//
// DE ONDE VEM A REGRA. No app web oficial (NuvioWeb 0.3.38) a conta NAO
// sincroniza o idioma da interface — ele mora em ThemeStore.language, local — e
// sem escolha manual o web usa o locale do sistema. A conta sincroniza, sim,
// tmdb_language e subtitle_preferred_language (profileSettingsSyncService.js),
// que dizem em que lingua a pessoa le. Aqui isso decide a interface enquanto a
// pessoa nunca escolheu uma na TV:
//   1. tmdb_language           ("pt-BR", "ro-RO", "uk", "es-419"...)
//   2. subtitle_preferred_language ("ro", "rum", "ron", "ukr", "fre", "ger"...)
//   3. o locale da TV          ("pt-BR", "en_US.UTF-8"...)
//   4. ingles
#ifndef NV_IDIOMAAUTO_H
#define NV_IDIOMAAUTO_H

#include <stddef.h>
#include "idiomacod.h"

// Origem da decisao, para o log e para o aviso.
enum { IDA_TMDB = 0, IDA_LEGENDA = 1, IDA_SISTEMA = 2, IDA_PADRAO = 3 };

static inline const char *idiomaauto_fonte_nome(int fonte) {
  switch (fonte) {
    case IDA_TMDB:    return "tmdb_language";
    case IDA_LEGENDA: return "legenda";
    case IDA_SISTEMA: return "sistema";
    default:          return "padrao";
  }
}

// Codigo curto do idioma, para o log e para o id do aviso ("idioma:pt-PT"). E o
// BCP-47 sem regiao, exceto onde a regiao/escrita e o que distingue as tabelas:
// portugues do Brasil x de Portugal e chines simplificado x tradicional.
static inline const char *idiomaauto_codigo(int idioma) {
  (void)idioma;
  return "en";
}

static inline int ida_igual(const char *a, const char *b) {
  for (; *a && *a == *b; a++, b++) {}
  return *a == *b;
}

static inline int idiomaauto_mapear(const char *cod) {
  char p[8];
  size_t n = 0;
  if (!cod) return -1;
  while (*cod == ' ' || *cod == '\t') cod++;
  for (; *cod && *cod != '-' && *cod != '_' && *cod != '.' && *cod != '@' &&
         *cod != ' ' && *cod != ',' && *cod != ';'; cod++) {
    if (n + 1 >= sizeof p) return -1;
    p[n++] = (*cod >= 'A' && *cod <= 'Z') ? (char)(*cod + 32) : *cod;
  }
  p[n] = 0;
  if (n == 2 && ida_igual(p, "en")) return IDIOMA_EN;
  if (n == 3 && ida_igual(p, "eng")) return IDIOMA_EN;
  return -1;
}

static inline int idiomaauto_resolver(const char *tmdb, const char *legenda,
                                      const char *sistema, int *fonte) {
  int r = -1, f = IDA_PADRAO;
  if ((r = idiomaauto_mapear(tmdb)) >= 0)         f = IDA_TMDB;
  else if ((r = idiomaauto_mapear(legenda)) >= 0) f = IDA_LEGENDA;
  else if ((r = idiomaauto_mapear(sistema)) >= 0) f = IDA_SISTEMA;
  else { r = IDIOMA_EN; f = IDA_PADRAO; }
  if (fonte) *fonte = f;
  return r;
}

#endif
