// A tabela de traducao esta ORDENADA e responde?
// Confere que toda chave de idioma_tab.h traduz para o ingles esperado,
// com os MESMOS marcadores printf e os mesmos \n que a chave.
#include "idioma.h"
#include "idiomacod.h"
#include "comentordem.h"
#include <stdio.h>
#include <string.h>

// Dubles: o teste nao sobe ajustes.c inteiro so para saber o idioma.
static int lg = IDIOMA_EN;
int ajustes_idioma(void) { return lg; }
int ajustes_idioma_ingles(void) { return 1; }

// As chaves, para varrer a tabela inteira sem tocar em idioma.c.
static const struct { const char *pt, *en; } CHAVES[] = {
#include "idioma_tab.h"
};

static int falhas;
static void confere(const char *pt, const char *esperado) {
  const char *r = i18n(pt);
  if (strcmp(r, esperado) != 0) {
    printf("FALHOU: \"%s\" -> \"%s\" (esperava \"%s\")\n", pt, r, esperado);
    falhas++;
  }
}

static void confere_mes(int mes, const char *pt, const char *esperado) {
  const char *r = idioma_mes_data(mes, pt);
  if (strcmp(r, esperado) != 0) {
    printf("FALHOU: mes %d (\"%s\") -> \"%s\" (esperava \"%s\")\n", mes, pt, r, esperado);
    falhas++;
  }
}

// Marcadores printf de `s`, na ordem, um por linha de saida ("%d%s%.1f..."), sem
// os "%%". Comparados como texto: "%d" contra "%s" e diferente.
static void marcadores(const char *s, char *out, size_t cap) {
  size_t o = 0;
  while (*s) {
    const char *p;
    if (*s != '%') { s++; continue; }
    if (s[1] == '%') { s += 2; continue; }
    p = s + 1;
    while (*p && strchr("-+ #0123456789.hlzjt", *p)) p++;
    if (*p && strchr("diouxXeEfgGcsp", *p) && o + (size_t)(p - s) + 3 < cap) {
      memcpy(out + o, s, (size_t)(p - s) + 1); o += (size_t)(p - s) + 1; out[o++] = ';';
      s = p + 1;
    } else s++;
  }
  out[o] = 0;
}
static int quebras(const char *s) { int n = 0; for (; *s; s++) if (*s == '\n') n++; return n; }

static void varredura(void) {
  size_t i, n = sizeof CHAVES / sizeof *CHAVES;
  for (i = 0; i < n; i++) {
    const char *r = i18n(CHAVES[i].pt);
    char mk[128], mr[128];
    if (!r || !*r) { printf("FALHOU: sem traducao de \"%s\"\n", CHAVES[i].pt); falhas++; continue; }
    if (strcmp(r, CHAVES[i].en) != 0) {
      printf("FALHOU: en de \"%s\" -> \"%s\"\n", CHAVES[i].pt, r); falhas++; }
    marcadores(CHAVES[i].pt, mk, sizeof mk); marcadores(r, mr, sizeof mr);
    if (strcmp(mk, mr) != 0 || quebras(CHAVES[i].pt) != quebras(r)) {
      printf("FALHOU: marcadores/quebras de \"%s\": \"%s\"\n", CHAVES[i].pt, r); falhas++; }
    if (falhas > 30) return;
  }
}

int main(void) {
  // Uma de cada tela, incluindo as que tem acento no meio e aspas escapadas.
  confere("Continuar assistindo", "Continue Watching");
  confere("Ajustes", "Settings");
  confere("Quem está assistindo?", "Who's watching?");
  confere("Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.",
          "No direct source available. Use Reload to try again.");
  confere("Ficção científica", "Science Fiction");
  confere("Sáb", "Sat");
  confere("Mostrar \"Continuar assistindo\"", "Show \"Continue Watching\"");

  // Os selos da guia parental.
  confere("Nudez", "Nudity");
  confere("Violência", "Violence");
  confere("Leve", "Mild");
  confere("Moderado", "Moderate");
  confere("Severo", "Severe");

  // Legendas montadas com %s.
  confere("Seleção do seu catálogo", "A selection from your catalog");
  confere("Conhecido por  %s", "Known for  %s");

  // Sem chave: devolve o proprio texto (titulo de filme).
  confere("Blade Runner 2049", "Blade Runner 2049");
  confere("", "");

  // Mes de uma data por extenso em ingles.
  confere_mes(7, "julho", "July");
  confere_mes(3, "mar\xc3\xa7o", "March");
  confere_mes(9, "setembro", "September");

  // Mes abreviado e data curta em ingles.
  { char o[48];
    if (strcmp(idioma_mes_curto(IDIOMA_EN, 0), "Jan") ||
        strcmp(idioma_mes_curto(IDIOMA_EN, 8), "Sep") ||
        strcmp(idioma_mes_curto(IDIOMA_EN, 11), "Dec")) {
      printf("FALHOU: mes curto en\n"); falhas++; }
    idioma_data_curta(IDIOMA_EN, 21, idioma_mes_curto(IDIOMA_EN, 8), 2025, o, sizeof o);
    if (strcmp(o, "Sep 21, 2025")) { printf("FALHOU: data curta en -> %s\n", o); falhas++; }
    idioma_data_curta(IDIOMA_EN, 21, idioma_mes_curto(IDIOMA_EN, 8), 0, o, sizeof o);
    if (strcmp(o, "Sep 21")) { printf("FALHOU: data curta en sem ano -> %s\n", o); falhas++; }
    idioma_mes_ano(IDIOMA_EN, "SEPTEMBER", 2026, o, sizeof o);
    if (strcmp(o, "SEPTEMBER 2026")) { printf("FALHOU: mes ano en -> %s\n", o); falhas++; }
  }

  // Caixa alta em UTF-8.
  { char o[32];
    idioma_maiusc(o, sizeof o, "settings");      if (strcmp(o, "SETTINGS")) { printf("FALHOU: maiusc settings\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "ao\xc3\xbbt");   if (strcmp(o, "AO\xc3\x9bT")) { printf("FALHOU: maiusc aout\n"); falhas++; }
    idioma_maiusc(o, sizeof o, "m\xc3\xa4rz");   if (strcmp(o, "M\xc3\x84RZ")) { printf("FALHOU: maiusc marz\n"); falhas++; }
  }

  // Separador decimal: ponto em ingles.
  if (!idioma_ponto_decimal(IDIOMA_EN)) {
    printf("FALHOU: idioma_ponto_decimal\n"); falhas++;
  }

  // GENEROS: todos na tabela mestra.
  { static const char *G[] = { "Ação", "Aventura", "Animação", "Biografia",
      "Comé" "dia", "Crime", "Documentário", "Drama", "Família", "Fantasia", "Noir",
      "Game show", "História", "Terror", "Música", "Musical", "Mistério", "Notícias",
      "Reality", "Romance", "Ficção científica", "Curta", "Esporte", "Talk show",
      "Suspense", "Guerra", "Faroeste", "Infantil", "Novela", "Adulto" };
    size_t i, k, n = sizeof CHAVES / sizeof *CHAVES;
    for (i = 0; i < sizeof G / sizeof *G; i++) {
      for (k = 0; k < n && strcmp(CHAVES[k].pt, G[i]); k++) {}
      if (k == n) { printf("FALHOU: genero \"%s\" nao esta na tabela\n", G[i]); falhas++; } } }

  // DETALHE DO TITULO.
  confere("Pa\xc3\xad" "ses Baixos", "Netherlands");
  confere("Lan\xc3\xa7" "ado", "Released");

  // Codigo ISO.
  if (strcmp(idioma_iso(IDIOMA_EN), "en")) {
    printf("FALHOU: idioma_iso(IDIOMA_EN)\n"); falhas++;
  }

  // COMENTARIOS: ordenacao com ingles na frente.
  { ComentAchado v[3];
    const char *L[3] = { "pt", "en", "es" };
    int i, k;
    memset(v, 0, sizeof v);
    for (i = 0; i < 3; i++) { snprintf(v[i].u, sizeof v[i].u, "u%d", i); strcpy(v[i].l, L[i]); }
    k = coment_ordenar(v, 3, "en");
    if (k != 1 || strcmp(v[0].u, "u1")) {
      printf("FALHOU: coment_ordenar en -> %s (k=%d)\n", v[0].u, k);
      falhas++;
    }
  }

  // Varredura da tabela inteira.
  varredura();

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("idioma ok\n");
  return 0;
}
