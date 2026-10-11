// #176: local search folding and complete-character backspace.
// Valid Latin, Romanian, Cyrillic and other scripts keep their behavior.
#include "../src/buscanorm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_equal(const char *input, const char *expected) {
  char d[128];
  busca_normalizar(input, d, sizeof d);
  if (strcmp(d, expected)) {
    printf("FAIL: \"%s\" -> \"%s\" (expected \"%s\")\n", input, d, expected);
    assert(0);
  }
}

int main(void) {
  // Latin-1 accents.
  check_equal("The Invite", "the invite");
  check_equal("Funda\xc3\xa7\xc3\xa3o", "fundacao");
  check_equal("A\xc3\xa7\xc3\xa3o \xc3\x89 Caf\xc3\xa9", "acao e cafe");
  puts("ok  Latin-1 folding unchanged");

  // Romanian comma/cedilla forms and Latin Extended-A.
  check_equal("\xc8\x98tefan \xc8\x9a\xc4\x83r\xc4\x83", "stefan tara");
  check_equal("\xc5\x9f\xc5\xa3 \xc3\xa2\xc3\xae \xc4\x82", "st ai a");
  check_equal("Pl\xc4\x83" "ce\xc8\x9b" "i", "placeti");
  puts("ok  Romanian accents fold to ASCII");

  // Cyrillic lowercase with ё folded to е.
  check_equal("\xd0\x94\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f",
        "\xd0\xb4\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f");
  check_equal("\xd0\x81\xd0\xbb\xd0\xba\xd0\xb0", "\xd0\xb5\xd0\xbb\xd0\xba\xd0\xb0");
  check_equal("\xd2\x90\xd0\xb0\xd0\xbd\xd0\xb3", "\xd2\x91\xd0\xb0\xd0\xbd\xd0\xb3");
  check_equal("\xd0\x86\xd0\xb3\xd0\xbe\xd1\x80", "\xd1\x96\xd0\xb3\xd0\xbe\xd1\x80");
  { char d[64];
    busca_normalizar("\xd0\x94\xd1\x80\xd1\x83\xd0\xb7\xd1\x8c\xd1\x8f", d, sizeof d);
    assert(strchr(d, ' ') == NULL); }
  puts("ok  Cyrillic lowercase and ё=е");

  check_equal("\xe6\x97\xa5\xe6\x9c\xac", "\xe6\x97\xa5\xe6\x9c\xac");
  check_equal("a\xff" "b", "a b");
  check_equal("", "");
  puts("ok  other scripts preserved; invalid byte becomes space");

  { char d[4];
    busca_normalizar("\xd0\x90\xd0\x91\xd0\x92", d, sizeof d);
    assert(strlen(d) == 2 && !strcmp(d, "\xd0\xb0")); }
  puts("ok  short output stops at character boundary");

  assert(busca_codepoints("abc") == 3);
  assert(busca_codepoints("\xd0\xb0\xd0\xb1") == 2);
  assert(busca_codepoints("") == 0);
  { char s[16] = "ab\xc8\x99";
    size_t n = busca_apagar_ultimo(s, strlen(s));
    assert(n == 2 && !strcmp(s, "ab"));
    n = busca_apagar_ultimo(s, n);
    assert(n == 1 && !strcmp(s, "a"));
    n = busca_apagar_ultimo(s, n); n = busca_apagar_ultimo(s, n);
    assert(n == 0 && s[0] == 0); }
  puts("ok  character counting and complete-character deletion");
  puts("buscanorm: ok");
  return 0;
}
