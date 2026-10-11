// Standalone JSON strings decode Unicode escapes from channel genre arrays.
// Python json.dumps defaults to ensure_ascii, so provider Unicode often arrives
// as escaped text. These multilingual provider values are preserved fixtures.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/js.h"

static const char *meta =
  "{\"id\":\"c1\",\"name\":\"Canal \\u00c1gua\",\"description\":\"Jornalismo 24h \\ud83d\\udcfa\","
  "\"genre\":[\"Not\\u00edcias\",\"Outro\"],\"genres\":[\"\\u65b0\\u95fb\"]}";

int main(void) {
  char b[64];
  const char *g;

  g = js_array(meta, NULL, "genre");
  assert(g && js_cadeia(g, b, sizeof b));
  assert(!strcmp(b, "Not\xc3\xad" "cias"));

  g = js_array(meta, NULL, "genres");          // Three UTF-8 bytes per character
  assert(g && js_cadeia(g, b, sizeof b));
  assert(!strcmp(b, "\xe6\x96\xb0\xe9\x97\xbb"));

  // Named and standalone fields share the same decoder
  assert(js_texto(meta, NULL, "name", b, sizeof b));
  assert(!strcmp(b, "Canal \xc3\x81gua"));
  assert(js_texto(meta, NULL, "description", b, sizeof b));
  assert(!strcmp(b, "Jornalismo 24h \xf0\x9f\x93\xba"));   // Surrogate pair

  // Ordinary and malformed escapes
  assert(js_cadeia("\"a\\\"b\\\\c\\/d\"", b, sizeof b) && !strcmp(b, "a\"b\\c/d"));
  assert(js_cadeia("\"x\\uZZ\"", b, sizeof b) && !strcmp(b, "x ZZ"));

  // Truncation backs up before a partial two-byte character
  assert(js_cadeia("\"abc\xc3\xad\"", b, 5) && !strcmp(b, "abc"));

  // Non-string inputs
  b[0] = 'q';
  assert(!js_cadeia("123", b, sizeof b));
  assert(!js_cadeia(NULL, b, sizeof b));
  puts("JSON strings: ok");
  return 0;
}
