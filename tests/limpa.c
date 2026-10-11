// #144: corpus output stays valid UTF-8 and uses Inter/fallback glyphs.
// Usage: test <Inter codepoints file> <tests/limpa_corpus.txt>.
// The runner extracts codepoints from the actual bundled font.
#include "../src/limpa.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *inter; // One flag per Unicode scalar slot.

static unsigned long next_codepoint(const unsigned char **pp) {
  const unsigned char *p = *pp;
  unsigned long cp; int n;
  if (*p < 0x80) { cp = *p; n = 1; }
  else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; n = 2; }
  else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; n = 3; }
  else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; n = 4; }
  else return 0xFFFFFFFFul;
  for (int i = 1; i < n; i++) {
    if ((p[i] & 0xC0) != 0x80) return 0xFFFFFFFFul;
    cp = cp << 6 | (p[i] & 0x3F);
  }
  *pp = p + n;
  return cp;
}

// Other BMP scripts are handled by text.c's fallback fonts.
static void check_glyphs(const char *s, const char *context) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned long cp = next_codepoint(&p);
    if (cp == 0xFFFFFFFFul) { printf("Invalid UTF-8 in: %s\n", context); abort(); }
    if (cp == '\n') continue;
    if (cp < 0x110000 && inter[cp]) continue;
    if (cp >= 0x370 && !(cp >= 0x2000 && cp <= 0x2BFF) && !(cp >= 0xFE00 && cp <= 0xFE0F) &&
        cp < 0x10000) continue;
    printf("Missing glyph U+%04lX in: %s\n", cp, context);
    abort();
  }
}

typedef struct { char in[1024], single[1024], multi[1024]; } CleanupCase;
static CleanupCase *cases;
static size_t case_count;

static void unescape(char *s) {
  char *w = s;
  for (; *s; s++) {
    if (*s == '\\' && s[1] == 'n') { *w++ = '\n'; s++; }
    else if (*s == '\\' && s[1] == 'r') { *w++ = '\r'; s++; }
    else if (*s == '\\' && s[1] == 't') { *w++ = '\t'; s++; }
    else if (*s == '\\' && s[1] == 'x' && s[2] && s[3]) {
      char h[3] = { s[2], s[3], 0 };
      *w++ = (char)strtoul(h, NULL, 16); s += 3;
    } else *w++ = *s;
  }
  *w = 0;
}

static void read_corpus(const char *path) {
  FILE *f = fopen(path, "r");
  char line[1024];
  int stage = 0;
  assert(f);
  cases = calloc(256, sizeof *cases);
  while (fgets(line, sizeof line, f)) {
    size_t n = strlen(line);
    if (n && line[n - 1] == '\n') line[--n] = 0;
    if (line[0] == '#' || (stage == 0 && !n)) continue;
    if (stage == 3) { assert(!strcmp(line, "--")); stage = 0; case_count++; continue; }
    unescape(line);
    strcpy(stage == 0 ? cases[case_count].in : stage == 1 ? cases[case_count].single : cases[case_count].multi, line);
    stage++;
  }
  fclose(f);
}

int main(int argc, char **argv) {
  char out[512];
  FILE *f;
  char line[32];
  size_t i;
  assert(argc == 3);
  read_corpus(argv[2]);
  inter = calloc(0x110000, 1);
  f = fopen(argv[1], "r");
  assert(f);
  while (fgets(line, sizeof line, f)) inter[strtoul(line, NULL, 16)] = 1;
  fclose(f);
  assert(inter['A'] && inter[0xE9]);

  assert(case_count >= 20);
  for (i = 0; i < case_count; i++) {
    nv_limpar_texto(cases[i].in, out, sizeof out, NV_LIMPA_UMA_LINHA);
    if (strcmp(out, cases[i].single)) {
      printf("case %zu (single line)\n  expected: %s\n  actual:   %s\n", i, cases[i].single, out);
      return 1;
    }
    check_glyphs(out, cases[i].single);
    nv_limpar_texto(cases[i].in, out, sizeof out, 0);
    if (strcmp(out, cases[i].multi)) {
      printf("case %zu (multiline)\n  expected: %s\n  actual:   %s\n", i, cases[i].multi, out);
      return 1;
    }
    check_glyphs(out, cases[i].multi);

    // Cleanup is idempotent for the formatter corpus.
    { char again[512];
      nv_limpar_texto(out, again, sizeof again, 0);
      assert(!strcmp(out, again)); }
  }

  // Every small capacity must preserve UTF-8/glyph boundaries.
  for (i = 0; i < case_count; i++) {
    size_t capacity;
    for (capacity = 1; capacity < 64; capacity++) {
      char small[64];
      size_t n = nv_limpar_texto(cases[i].in, small, capacity, NV_LIMPA_UMA_LINHA);
      assert(n < capacity && small[n] == 0);
      check_glyphs(small, "truncation");
    }
  }

  assert(nv_limpar_texto(NULL, out, sizeof out, 0) == 0 && !out[0]);
  assert(nv_limpar_texto("", out, sizeof out, 0) == 0);
  assert(nv_limpar_texto("abc", out, 0, 0) == 0);
  { char one[1] = { 'x' }; assert(nv_limpar_texto("abc", one, 1, 0) == 0 && !one[0]); }

  // Trim standalone/edge separators, preserving interior release-name hyphens.
  { static const char *const C[][2] = {
      { "31.4 GB | 43.2 Mbps |", "31.4 GB | 43.2 Mbps" },
      { "| A |  | B |", "A | B" },
      { "  -  A  \xE2\x80\xA2 \xC2\xB7 B / ", "A \xC2\xB7 B" },
      { "WEB-DL x265-ELiTE", "WEB-DL x265-ELiTE" },
      { "EN | PT,", "EN | PT" },
      { "|", "" }, { " | - ", "" }, { "", "" },
      { "A   B", "A B" },
    };
    for (i = 0; i < (int)(sizeof C / sizeof C[0]); i++) {
      char b[128];
      snprintf(b, sizeof b, "%s", C[i][0]);
      nv_aparar_separadores(b);
      if (strcmp(b, C[i][1])) { fprintf(stderr, "trim: '%s' -> '%s' (want '%s')\n", C[i][0], b, C[i][1]); return 1; }
    } }
  free(inter);
  free(cases);
  puts("limpa: ok");
  return 0;
}
