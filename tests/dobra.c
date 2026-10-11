// #144: provider-styled letters fold to ordinary characters.
// Preserve the existing multilingual fixtures and expected mappings.
#include "../src/dobra.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *fold_text(const char *s) {
  static char out[256];
  const unsigned char *p = (const unsigned char *)s;
  int k = 0;
  while (*p) {
    unsigned long cp; int n;
    if (*p < 0x80) { cp = *p; n = 1; }
    else if ((*p & 0xE0) == 0xC0) { cp = (unsigned long)(*p & 0x1F) << 6 | (p[1] & 0x3F); n = 2; }
    else if ((*p & 0xF0) == 0xE0) { cp = (unsigned long)(*p & 0x0F) << 12 | (unsigned long)(p[1] & 0x3F) << 6 | (p[2] & 0x3F); n = 3; }
    else { cp = (unsigned long)(*p & 0x07) << 18 | (unsigned long)(p[1] & 0x3F) << 12 | (unsigned long)(p[2] & 0x3F) << 6 | (p[3] & 0x3F); n = 4; }
    { char c = nv_dobra_estilizada(cp);
      if (c) out[k++] = c;
      else { int i; for (i = 0; i < n; i++) out[k++] = (char)p[i]; } }
    p += n;
  }
  out[k] = 0;
  return out;
}

int main(void) {
  assert(nv_dobra_estilizada('A') == 0);
  assert(nv_dobra_estilizada(0x00E9) == 0);
  assert(nv_dobra_estilizada(0x2022) == 0);
  assert(!strcmp(fold_text("R\u1D07L\u1D07A\uA731\u1D07"), "RELEASE"));
  assert(!strcmp(fold_text("S\u2080\u2081\u1D07\u2080\u2088"), "S01E08"));
  assert(!strcmp(fold_text("\U0001D5D5\U0001D5F9\U0001D602\U0001D5E5\U0001D5EE\U0001D606"), "BluRay"));
  assert(!strcmp(fold_text("\u24B9\u24E4\u24D1"), "Dub"));
  assert(!strcmp(fold_text("\uFF28\uFF24\uFF32 \uFF14\uFF2B"), "HDR 4K"));
  assert(!strcmp(fold_text("\U0001D7EE\U0001D7ED\U0001D7F2\U0001D7ECp"), "2160p"));
  assert(!strcmp(fold_text("\u029C\u1D07\u1D20\u1D04"), "HEVC"));
  assert(!strcmp(fold_text("\u1D34\u1D30 \u02B0\u1D48"), "HD hd"));
  assert(!strcmp(fold_text("\U0001F170\U0001F171"), "AB"));
  assert(!strcmp(fold_text("Fundação 1080p"), "Fundação 1080p"));
  puts("dobra: ok");
  return 0;
}
