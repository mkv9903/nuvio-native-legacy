// Real C callers and Rust staticlib against the retained C text algorithms.
#include "dobra.h"
#include "limpa.h"
#include "buscanorm.h"
#include "rust_core.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

char reference_fold(unsigned long);
size_t reference_cleanup(const char *, char *, size_t, int);
size_t reference_trim(char *);
void reference_normalize(const char *, char *, size_t);
int reference_count(const char *);
size_t reference_delete(char *, size_t);

static unsigned long comparisons, malformed_cases;
static uint32_t random_state = UINT32_C(0x194176);
static uint32_t next_random(void) {
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

static size_t encode(unsigned cp, char *out) {
  if (cp < 0x80) { out[0] = (char)cp; return 1; }
  if (cp < 0x800) { out[0] = (char)(0xc0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); return 2; }
  if (cp < 0x10000) {
    out[0] = (char)(0xe0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 63));
    out[2] = (char)(0x80 | (cp & 63)); return 3;
  }
  out[0] = (char)(0xf0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 63));
  out[2] = (char)(0x80 | ((cp >> 6) & 63)); out[3] = (char)(0x80 | (cp & 63)); return 4;
}

// Independent strict decoder for output validation and approved invalid-byte
// replacement. It uses byte-range constraints rather than Rust's scalar check.
static size_t valid_width(const unsigned char *p, size_t n) {
  if (p[0] < 0x80) return 1;
  size_t width = p[0] >= 0xc2 && p[0] <= 0xdf ? 2 :
                 p[0] >= 0xe0 && p[0] <= 0xef ? 3 :
                 p[0] >= 0xf0 && p[0] <= 0xf4 ? 4 : 0;
  if (!width || width > n) return 0;
  for (size_t i = 1; i < width; i++) if ((p[i] & 0xc0) != 0x80) return 0;
  if ((p[0] == 0xe0 && p[1] < 0xa0) || (p[0] == 0xed && p[1] >= 0xa0) ||
      (p[0] == 0xf0 && p[1] < 0x90) || (p[0] == 0xf4 && p[1] >= 0x90)) return 0;
  return width;
}
static void check_utf8(const char *s) {
  size_t n = strlen(s), pos = 0;
  while (pos < n) { size_t width = valid_width((const unsigned char *)s + pos, n - pos); assert(width); pos += width; }
}
static void sanitize_search(const char *input, char *out) {
  size_t n = strlen(input), pos = 0;
  while (pos < n) {
    size_t width = valid_width((const unsigned char *)input + pos, n - pos);
    if (!width) { out[pos++] = ' '; }
    else { memcpy(out + pos, input + pos, width); pos += width; }
  }
  out[pos] = 0;
}
static void check_cleanup(const char *input, size_t capacity, int flags) {
  unsigned char actual[260], expected[260];
  memset(actual, 0xa5, sizeof actual); memset(expected, 0xa5, sizeof expected);
  size_t a = nv_limpar_texto(input, (char *)actual + 1, capacity, flags);
  size_t b = reference_cleanup(input, (char *)expected + 1, capacity, flags);
  if (a != b || memcmp(actual, expected, sizeof actual)) {
    fprintf(stderr, "cleanup mismatch at comparison %lu, capacity %zu, flags %d\n", comparisons, capacity, flags); assert(0);
  }
  if (capacity) { assert(a < capacity); check_utf8((char *)actual + 1); }
  comparisons++;
}
static void check_search(const char *input, size_t capacity, int malformed) {
  unsigned char actual[260], expected[260];
  char sanitized[256];
  memset(actual, 0xa5, sizeof actual); memset(expected, 0xa5, sizeof expected);
  const char *oracle = input;
  if (malformed) { sanitize_search(input, sanitized); oracle = sanitized; }
  busca_normalizar(input, (char *)actual + 1, capacity);
  reference_normalize(oracle, (char *)expected + 1, capacity);
  if (memcmp(actual, expected, sizeof actual)) {
    fprintf(stderr, "search mismatch at comparison %lu, capacity %zu\n", comparisons, capacity); assert(0);
  }
  if (capacity) check_utf8((char *)actual + 1);
  // The same-address C contract remains supported, including small capacity.
  unsigned char inplace[260]; memset(inplace, 0xa5, sizeof inplace);
  size_t length = strlen(input); memcpy(inplace + 1, input, length + 1);
  busca_normalizar((char *)inplace + 1, (char *)inplace + 1, capacity);
  assert(inplace[0] == 0xa5 && inplace[259] == 0xa5);
  if (capacity) assert(!strcmp((char *)inplace + 1, (char *)expected + 1));
  else assert(!strcmp((char *)inplace + 1, input));
  comparisons += 2;
  if (malformed) malformed_cases++;
}
static void check_editing(const char *input) {
  unsigned char actual[260], expected[260];
  memset(actual, 0xa5, sizeof actual); memset(expected, 0xa5, sizeof expected);
  memcpy(actual + 1, input, strlen(input) + 1); memcpy(expected + 1, input, strlen(input) + 1);
  assert(busca_codepoints(input) == reference_count(input)); comparisons++;
  size_t n = strlen(input);
  do {
    size_t a = busca_apagar_ultimo((char *)actual + 1, n);
    size_t b = reference_delete((char *)expected + 1, n);
    assert(a == b && !memcmp(actual, expected, sizeof actual));
    n = a; comparisons++;
  } while (n);
}
static void check_trim(const char *input) {
  unsigned char actual[260], expected[260];
  memset(actual, 0xa5, sizeof actual); memset(expected, 0xa5, sizeof expected);
  memcpy(actual + 1, input, strlen(input) + 1); memcpy(expected + 1, input, strlen(input) + 1);
  size_t a = nv_aparar_separadores((char *)actual + 1);
  size_t b = reference_trim((char *)expected + 1);
  assert(a == b && !memcmp(actual, expected, sizeof actual)); comparisons++;
}

int main(void) {
  // Exhaustive styled mapping and valid Unicode scalar behavior through C ABI.
  for (unsigned cp = 0; cp <= 0x10ffff; cp++) {
    assert(nv_dobra_estilizada(cp) == reference_fold(cp)); comparisons++;
    if (!cp || (cp >= 0xd800 && cp <= 0xdfff)) continue;
    char input[16] = "A "; size_t n = encode(cp, input + 2);
    memcpy(input + 2 + n, " B", 3);
    check_cleanup(input, 64, (int)(cp & 1));
    check_search(input, 16, 0);
  }
  assert(nv_dobra_estilizada(ULONG_MAX) == reference_fold(ULONG_MAX)); comparisons++;
  static const char *const edges[] = {
    "", "A ⚡ )", "(🌐) A [⚡]", "A\n\nB", "🇬🇧🇧🇷🇿🇿🇦", "A • · | - / , B",
    "|A B,", "WEB-DL x265-ELiTE", "日本 Друзья ȘȚ ᴴᴰ ＨＤＲ",
    "A\xff" "Z", "A\xe0\x80\x80Z", "A\xed\xa0\x80Z",
    "A\xf0\x80\x80\x80Z", "A\xf4\x90\x80\x80Z", "A\xc2", "A\xe2\x28\xa1Z",
    "A\xef\xbf\xbdZ", "A\x80\xbfZ", "A\xf5\x80\x80\x80Z"
  };
  for (size_t i = 0; i < sizeof edges / sizeof *edges; i++) {
    for (size_t capacity = 0; capacity <= 64; capacity++) {
      check_cleanup(edges[i], capacity, 0); check_cleanup(edges[i], capacity, 1);
      check_search(edges[i], capacity, 1);
    }
    check_trim(edges[i]); check_editing(edges[i]);
  }
  // Seeded mixtures exercise interactions, malformed bytes and guard regions.
  static const unsigned cp_pool[] = { 'A', ' ', '\n', '\t', '|', '-', '/', ',', '(', ')', '[', ']',
    0xb7, 0x2022, 0x2705, 0x1f1ec, 0x1f1e7, 0x1f1e6, 0x1f1f7, 0x1f31f, 0x1d07,
    0xff28, 0x34f, 0x332, 0x200b, 0xfffd, 0x2600, 0x65e5, 0x42f, 0x218, 0x10000 };
  for (unsigned sample = 0; sample < 6000; sample++) {
    char input[256]; size_t n = 0;
    for (unsigned i = 0; i < 24; i++) {
      if (sample & 1) n += encode(cp_pool[next_random() % (sizeof cp_pool / sizeof *cp_pool)], input + n);
      else input[n++] = (char)(1 + next_random() % 255);
    }
    input[n] = 0;
    size_t capacity = next_random() % 128;
    check_cleanup(input, capacity, (int)(next_random() & 1));
    check_search(input, capacity, !(sample & 1));
    check_trim(input); check_editing(input);
  }
  // Bounded null/zero-output contracts and safe defensive null editing.
  char output[8] = "x";
  assert(nv_limpar_texto(NULL, output, sizeof output, 0) == 0 && !output[0]);
  assert(nv_limpar_texto("A", NULL, 8, 0) == 0);
  busca_normalizar(NULL, output, sizeof output); assert(!output[0]);
  busca_normalizar("A", NULL, 8); busca_normalizar(NULL, NULL, 0);
  assert(busca_codepoints(NULL) == 0);
  assert(nuvio_rust_search_delete_last(NULL, 0) == 0);
  assert(nuvio_rust_text_trim_separators(NULL) == 0);
  printf("text helpers: PASS (%lu comparisons; %lu search cases use the approved strict UTF-8 rule)\n", comparisons, malformed_cases);
  return 0;
}
