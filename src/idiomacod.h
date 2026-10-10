// Codigos dos idiomas da interface e os pedacos de texto que dependem so do
// idioma (mes abreviado, caixa alta em UTF-8). Sem dependencia de ajustes.h:
// cwordem.c e noticias.c sao compilados isolados por testes e nao podem puxar
// a biblioteca grafica so por causa de um numero.
//
// O NUMERO E O QUE FICA GRAVADO em ajustes.txt (AJ_IDIOMA): 0 e 1 sao os dois
// idiomas de sempre, e ficaram onde estavam para nao trocar o idioma de quem ja
// escolheu. Novos entram no FIM; nunca reordenar.
#ifndef NV_IDIOMACOD_H
#define NV_IDIOMACOD_H

#include <stddef.h>
#include <stdio.h>

enum { IDIOMA_EN = 0, IDIOMA_N = 1, IDIOMA_PT = 0 };

static inline const char *idioma_iso(int idioma) {
  (void)idioma;
  return "en";
}

// 1 = a interface escreve o decimal com PONTO ("8.4"); 0 = com virgula ("8,4").
// Em ingles usa ponto.
static inline int idioma_ponto_decimal(int idioma) {
  (void)idioma;
  return 1;
}

static inline void idioma_decimal_texto(char *s, int idioma) {
  (void)s;
  (void)idioma;
}

static inline int idioma_data_asiatica(int idioma) {
  (void)idioma;
  return 0;
}

// Mes abreviado, m0 = 0..11.
static inline const char *idioma_mes_curto(int idioma, int m0) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  (void)idioma;
  if (m0 < 0 || m0 > 11) return "";
  return EN[m0];
}

// Data curta ("Sep 21, 2026", "Sep 21").
static inline void idioma_data_curta(int idioma, int dia, const char *mes, int ano,
                                     char *dst, size_t cap) {
  (void)idioma;
  if (!dst || !cap) return;
  if (ano) snprintf(dst, cap, "%s %d, %d", mes, dia, ano);
  else     snprintf(dst, cap, "%s %d", mes, dia);
}

static inline int idioma_data_extenso_especial(int idioma, int dia, int mes,
                                               const char *nome, const char *ano,
                                               char *dst, size_t cap) {
  (void)idioma; (void)dia; (void)mes; (void)nome; (void)ano; (void)dst; (void)cap;
  return 0;
}

static inline void idioma_mes_ano(int idioma, const char *nome, int ano, char *dst, size_t cap) {
  (void)idioma;
  if (!dst || !cap) return;
  snprintf(dst, cap, "%s %d", nome, ano);
}

// Caixa alta em UTF-8.
static inline size_t idioma_maiusc_em(int idioma, char *dst, size_t tam, const char *s) {
  size_t i = 0, o = 0;
  (void)idioma;
  if (!tam) return 0;
  dst[0] = 0;
  if (!s) return 0;
  while (s[i] && o + 3 < tam) {
    unsigned char c = (unsigned char)s[i], d = (unsigned char)s[i + 1];
    unsigned char a = c, b = d, e2 = 0;
    int n = 1;
    if (c < 0x80) {
      if (c >= 'a' && c <= 'z') a = (unsigned char)(c - 32);
    } else if ((c & 0xF0) == 0xE0 && d && s[i + 2]) {
      // 3 bytes: vietnamita (U+1EA0-1EF9, minuscula = impar) e nada mais.
      unsigned char f = (unsigned char)s[i + 2];
      n = 3; e2 = f;
      if (c == 0xE1 && (d == 0xBA || d == 0xBB) && (f & 1)) {
        int cp = 0x1000 | ((d & 0x3F) << 6) | (f & 0x3F);
        if (cp >= 0x1EA1 && cp <= 0x1EF9) e2 = (unsigned char)(f - 1);
      }
    } else if (d) {
      n = 2;
      if (c == 0xC3 && d >= 0xA0 && d <= 0xBE && d != 0xB7) b = (unsigned char)(d - 0x20);
      else if (c == 0xC3 && d == 0xBF) { a = 0xC5; b = 0xB8; }         /* ÿ */
      else if (c == 0xC4 && d == 0xB1) { dst[o++] = 'I'; i += 2; continue; }  /* ı */
      // Latin Estendido-A (U+0100-017F): pares maiuscula/minuscula, com a
      // paridade INVERTIDA entre U+0139 e U+0148 e de U+0179 a U+017E. O
      // caso do "ŀ" (U+0140, o par cruza o byte) fica de fora: so o catalao usa.
      else if (c == 0xC4 && d >= 0x80 && d <= 0xBF) {
        int cp = 0x100 + (d - 0x80);
        if (cp <= 0x137 ? ((cp & 1) && cp != 0x131) : (cp >= 0x13A && !(cp & 1))) b = (unsigned char)(d - 1);
      }
      else if (c == 0xC5 && d >= 0x80 && d <= 0xBF) {
        int cp = 0x140 + (d - 0x80);
        if (cp <= 0x148 ? (cp >= 0x142 && !(cp & 1))
                        : cp >= 0x14B && cp <= 0x177 ? (cp & 1)
                        : (cp >= 0x17A && cp <= 0x17E && !(cp & 1))) b = (unsigned char)(d - 1);
      }
      else if (c == 0xC6 && d == 0xA1) b = 0xA0;                     /* ơ */
      else if (c == 0xC6 && d == 0xB0) b = 0xAF;                     /* ư */
      else if (c == 0xC8 && (d == 0x99 || d == 0x9B)) b = (unsigned char)(d - 1); /* ș ț */
      else if (c == 0xCE && d == 0x86) b = 0x91;                     /* Ά */
      else if (c == 0xCE && (d == 0x88 || d == 0x89 || d == 0x8A)) b = (unsigned char)(d + (d == 0x88 ? 0x0D : d == 0x89 ? 0x0E : 0x0F));
      else if (c == 0xCE && d == 0x8C) b = 0x9F;                     /* Ό */
      else if (c == 0xCE && d == 0x8E) b = 0xA5;                     /* Ύ */
      else if (c == 0xCE && d == 0x8F) b = 0xA9;                     /* Ώ */
      else if (c == 0xCE && (d == 0x90 || d == 0xB0)) b = d == 0x90 ? 0x99 : 0xA5; /* ΐ ΰ */
      else if (c == 0xCE && d == 0xAC) b = 0x91;                     /* ά */
      else if (c == 0xCE && d == 0xAD) b = 0x95;                     /* έ */
      else if (c == 0xCE && d == 0xAE) b = 0x97;                     /* ή */
      else if (c == 0xCE && d == 0xAF) b = 0x99;                     /* ί */
      else if (c == 0xCE && d >= 0xB1 && d <= 0xBF) b = (unsigned char)(d - 0x20);   /* α..ο */
      else if (c == 0xCF && (d == 0x80 || d == 0x81)) { a = 0xCE; b = (unsigned char)(d + 0x20); } /* π ρ */
      else if (c == 0xCF && (d == 0x82 || d == 0x83)) { a = 0xCE; b = 0xA3; }         /* ς σ */
      else if (c == 0xCF && d >= 0x84 && d <= 0x89) { a = 0xCE; b = (unsigned char)(d + 0x20); } /* τ..ω */
      else if (c == 0xCF && d == 0x8A) { a = 0xCE; b = 0x99; }       /* ϊ */
      else if (c == 0xCF && d == 0x8B) { a = 0xCE; b = 0xA5; }       /* ϋ */
      else if (c == 0xCF && d == 0x8C) { a = 0xCE; b = 0x9F; }       /* ό */
      else if (c == 0xCF && d == 0x8D) { a = 0xCE; b = 0xA5; }       /* ύ */
      else if (c == 0xCF && d == 0x8E) { a = 0xCE; b = 0xA9; }       /* ώ */
      else if (c == 0xD0 && d >= 0xB0 && d <= 0xBF) b = (unsigned char)(d - 0x20);
      else if (c == 0xD1 && d >= 0x80 && d <= 0x8F) { a = 0xD0; b = (unsigned char)(d + 0x20); }
      else if (c == 0xD1 && (d == 0x91 || d == 0x94 || d == 0x96 || d == 0x97)) {
        a = 0xD0; b = (unsigned char)(d - 0x10); }                    /* ё є і ї */
      else if (c == 0xD2 && d == 0x91) b = 0x90;                     /* ґ */
    }
    dst[o++] = (char)a;
    if (n >= 2) dst[o++] = (char)(n == 3 ? d : b);
    if (n == 3) dst[o++] = (char)e2;
    i += (size_t)n;
    /* bytes 3 e 4 de uma sequencia longa passam crus no laco seguinte: nenhum
       deles cai nas faixas acima, porque sao bytes de continuacao (0x80-0xBF). */
  }
  dst[o] = 0;
  return o;
}

// Mesma coisa, no idioma que nao muda o "i" (todos menos o turco).
static inline size_t idioma_maiusc(char *dst, size_t tam, const char *s) {
  return idioma_maiusc_em(IDIOMA_EN, dst, tam, s);
}

#endif
