// Real C/Rust ABI comparisons, bounded allocations and the approved prefix fix.
#include "js.h"
#include "rust_core.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

const char *reference_json_end(const char *);
int reference_json_has(const char *, const char *, const char *);
int reference_json_string(const char *, char *, size_t);
int reference_json_text(const char *, const char *, const char *, char *, size_t);
int reference_json_lines(const char *, const char *, const char *, char *, size_t);
double reference_json_number(const char *, const char *, const char *, double);
const char *reference_json_array(const char *, const char *, const char *);
const char *reference_json_next(const char *);
const char *reference_json_root_array(const char *);
int reference_json_raw(const char *, const char *, const char *, char *, size_t);
long long reference_json_iso_ms(const char *);
int reference_json_root_text(const char *, const char *, char *, size_t);
int reference_json_root_text_range(const char *, const char *, const char *, char *, size_t);

static size_t comparisons;
static void check(int condition) {
  comparisons++;
  if (!condition) { fprintf(stderr, "JSON comparison %zu failed\n", comparisons); abort(); }
}

typedef int (*TextReader)(const char *, const char *, const char *, char *, size_t);
static void compare_text(TextReader actual, TextReader reference, const char *input,
                         const char *end, const char *key, size_t capacity) {
  unsigned char first[544], second[544];
  memset(first, 0x55, sizeof first);
  memcpy(second, first, sizeof first);
  check(actual(input, end, key, (char *)first + 8, capacity) ==
        reference(input, end, key, (char *)second + 8, capacity));
  check(!memcmp(first, second, sizeof first));
}

static void compare_range(const char *input, const char *end, size_t capacity) {
  const char *keys[] = {"x", "n", "v", "name", "id", "", "missing"};
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
    const char *key = keys[i];
    compare_text(js_texto, reference_json_text, input, end, key, capacity);
    compare_text(js_texto_linhas, reference_json_lines, input, end, key, capacity);
    compare_text(js_texto_raiz_em, reference_json_root_text_range, input, end, key, capacity);
    compare_text(js_bruto, reference_json_raw, input, end, key, capacity);
    check(js_tem(input, end, key) == reference_json_has(input, end, key));
    double first = js_num(input, end, key, -1234.5);
    double second = reference_json_number(input, end, key, -1234.5);
    check(first == second && signbit(first) == signbit(second));
    check(js_array(input, end, key) == reference_json_array(input, end, key));
  }
}

static void document_fixtures(void) {
  const char *cases[] = {
    "", "{}", "[]", "[1,2]", "[ {\"x\":\"first\"}, {\"x\":\"second\"}]",
    "{\"x\":\"a\\n\\r\\t\\q\\b\\f\\/\\\\\\\"z\",\"n\":\"8.1\",\"v\":[{},\"string\"]}",
    "{\"x\":\"Not\\u00edcias \\uD83D\\uDE00\",\"n\":12.5e2,\"v\":[{\"x\":\"a\"}]}",
    "{\"nested\":{\"name\":\"inner\",\"id\":\"catalog\"},\"name\":\"outer\",\"id\":\"manifest\"}",
    "{\"x\":\"\\uZZ\\ud800\\uDC00\\ud800\\u0041\\u0000x\"}",
    "{\"x\":\"日é😀AB\",\"n\":{\"n\":8580},\"v\":{\"a\":1}}",
    "{\"literal\":\"\\\"x\\\":\\\"fake\\\"\",\"x\":\"real\"}",
    "{\"n\":1e999}", "{\"n\":\"1e+\"}", "{\"n\":-0.0}", "{\"n\":1-2}",
    "{\"n\":+1,\"n\":2}", "{\"n\":\"nan\",\"n\":\".5\"}",
    "{\"x\":null,\"v\":true   }", "{\"v\":}", "{\"x\":\"\"}",
    "{\"x\":\"\\", "{\"x\":\"\\uD800\\u", "{\"v\":{\"x\":\"}\\\"[\"}}",
    "prefix {\"x\":\"root\"} trailer", "{\"v\":{[{}]}}", "{\"v\":[{]}]}",
    "{\"x\":\"\xc0\x80\xff\"}", "{\"x\":\"escaped\\\"x\",\"v\":123}",
    "{\"x\":\"value\"}\0{\"name\":\"after NUL\"}"
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    const char *input = cases[i];
    size_t length = strlen(input);
    check(js_fim(input) == reference_json_end(input));
    check(js_raiz_array(input) == reference_json_root_array(input));
    for (size_t n = 0; n <= length; n++) {
      char *bounded = malloc(n ? n : 1);
      assert(bounded);
      memcpy(bounded, input, n);
      compare_range(bounded, bounded + n, 512);
      // Small-capacity C comparisons exclude the approved escaped-prefix bug.
      if (!strstr(input, "\\u")) {
        for (size_t capacity = 0; capacity < 24; capacity++) {
          compare_range(bounded, bounded + n, capacity);
        }
      }
      free(bounded);
    }
    for (size_t n = 0; n <= length; n++) {
      check(js_prox(input + n) == reference_json_next(input + n));
    }
    char first[512], second[512];
    memset(first, 0x55, sizeof first); memcpy(second, first, sizeof first);
    check(js_cadeia(input, first, sizeof first) == reference_json_string(input, second, sizeof second));
    check(!memcmp(first, second, sizeof first));
    memset(first, 0x55, sizeof first); memcpy(second, first, sizeof first);
    check(js_texto_raiz(input, "name", first, sizeof first) == reference_json_root_text(input, "name", second, sizeof second));
    check(!memcmp(first, second, sizeof first));
  }
  const char embedded[] = "{\"x\":\"one\"}\0{\"name\":\"two\"}";
  compare_range(embedded, embedded + sizeof embedded - 1, 512);
}

static size_t encode_scalar(uint32_t value, char *output) {
  if (value < 0x80) { output[0] = (char)value; return 1; }
  if (value < 0x800) {
    output[0] = (char)(0xc0 | (value >> 6)); output[1] = (char)(0x80 | (value & 63)); return 2;
  }
  if (value < 0x10000) {
    output[0] = (char)(0xe0 | (value >> 12)); output[1] = (char)(0x80 | ((value >> 6) & 63));
    output[2] = (char)(0x80 | (value & 63)); return 3;
  }
  output[0] = (char)(0xf0 | (value >> 18)); output[1] = (char)(0x80 | ((value >> 12) & 63));
  output[2] = (char)(0x80 | ((value >> 6) & 63)); output[3] = (char)(0x80 | (value & 63)); return 4;
}

static void every_unicode_scalar(void) {
  for (uint32_t value = 0; value <= 0x10ffff; value++) {
    if (value >= 0xd800 && value <= 0xdfff) continue;
    char escaped[48], first[32], second[32];
    if (value < 0x10000) snprintf(escaped, sizeof escaped, "\"P\\u%04xAB\"", value);
    else {
      uint32_t pair = value - 0x10000;
      snprintf(escaped, sizeof escaped, "\"P\\u%04x\\u%04xAB\"", 0xd800 + (pair >> 10), 0xdc00 + (pair & 1023));
    }
    memset(first, 0x55, sizeof first); memcpy(second, first, sizeof first);
    check(js_cadeia(escaped, first, sizeof first) == reference_json_string(escaped, second, sizeof second));
    check(!memcmp(first, second, sizeof first));
    char scalar[4]; size_t width = encode_scalar(value, scalar);
    for (size_t capacity = 1; capacity <= 9; capacity++) {
      char output[32]; memset(output, 0x55, sizeof output);
      int result = js_cadeia(escaped, output + 4, capacity);
      char expected[16] = {0}; size_t length = 0;
      if (capacity > 1) {
        expected[length++] = 'P';
        if (length + width < capacity) {
          memcpy(expected + length, scalar, width); length += width;
          if (length + 1 < capacity) expected[length++] = 'A';
          if (length + 1 < capacity) expected[length++] = 'B';
        }
      }
      check(result == (length > 0));
      check(!memcmp(output + 4, expected, length + 1));
      for (size_t j = 0; j < 4; j++) check((unsigned char)output[j] == 0x55);
      for (size_t j = capacity + 4; j < sizeof output; j++) check((unsigned char)output[j] == 0x55);
    }
  }
  // Exercise the same approved fix through all named text entry points.
  const char *input = "{\"x\":\"P\\u65e5AB\"}";
  for (size_t capacity = 1; capacity < 9; capacity++) {
    char string[16] = {0}, named[16] = {0}, lines[16] = {0}, root[16] = {0};
    int result = js_cadeia("\"P\\u65e5AB\"", string, capacity);
    check(js_texto(input, NULL, "x", named, capacity) == result && !strcmp(string, named));
    check(js_texto_linhas(input, NULL, "x", lines, capacity) == result && !strcmp(string, lines));
    check(js_texto_raiz(input, "x", root, capacity) == result && !strcmp(string, root));
  }
}

static uint32_t random_state = 0x8412;
static uint32_t next_random(void) {
  random_state ^= random_state << 13; random_state ^= random_state >> 17;
  random_state ^= random_state << 5; return random_state;
}

static void malformed_bytes(void) {
  for (size_t iteration = 0; iteration < 3000; iteration++) {
    char input[192]; size_t length = next_random() % (sizeof input - 1);
    for (size_t i = 0; i < length; i++) input[i] = (char)(next_random() & 255);
    input[length] = 0;
    if (length >= 6) memcpy(input + next_random() % (length - 5), "\"x\":\"", 5);
    // Full capacity isolates compatibility from the one approved correction.
    compare_range(input, input + length, 512);
    check(js_fim(input) == reference_json_end(input));
    check(js_raiz_array(input) == reference_json_root_array(input));
    check(js_prox(input) == reference_json_next(input));
  }
}

static void protected_ranges(void) {
  long page = sysconf(_SC_PAGESIZE);
  assert(page > 512);
  char *allocation = mmap(NULL, (size_t)page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(allocation != MAP_FAILED);
  assert(!mprotect(allocation + page, (size_t)page, PROT_NONE));
  const char *input = "{\"x\":\"P\\ud83d\\ude00AB\",\"n\":\"8.1\",\"v\":[{\"x\":\"a\"}]}";
  for (size_t length = 0; length <= strlen(input); length++) {
    char *start = allocation + page - length;
    memcpy(start, input, length);
    compare_range(start, allocation + page, 512);
  }
  assert(!munmap(allocation, (size_t)page * 2));
}

static void timestamps_and_numeric_limits(void) {
  const char *dates[] = {NULL, "", "wrong", "1970-01-01", "1969-12-31T23:59:59Z",
    "2026-09-04T18:52:07.123456+00:00", "2026-09-04 18:52:07.1", "2026-09-04T18:52:07.12Z",
    "2026-09-04T18:52:07.Z", "2026-09-04T18:52:07+05:30", "2026-09-04X18:52:07",
    "2026-09-04T18", "2026-09-04T18:52", "2026-13-35T25:65:70.4567Z", "2026-00-00",
    "2038-01-19T03:14:07Z", "2038-01-19T03:14:08Z", "2040-01-01T00:00:00Z",
    " 2026- 9- 4T -1: -2: -3.9", "2026-09-04suffix.999", "2026-09-04T18:52:07.123456-02:00"};
  for (size_t i = 0; i < sizeof dates / sizeof dates[0]; i++) check(js_ms_iso(dates[i]) == reference_json_iso_ms(dates[i]));
  for (int year = 1968; year <= 2042; year++) {
    for (int month = 0; month <= 13; month++) {
      char date[96]; snprintf(date, sizeof date, "%d-%d-32T25:61:70.123456Z", year, month);
      check(js_ms_iso(date) == reference_json_iso_ms(date));
    }
  }
  for (size_t digits = 1; digits <= 140; digits++) {
    char input[160] = "{\"n\":"; memset(input + 5, '1', digits); strcpy(input + 5 + digits, "}");
    check(js_num(input, NULL, "n", -1) == reference_json_number(input, NULL, "n", -1));
  }
  check(js_fim(NULL) == NULL && js_prox(NULL) == NULL && js_raiz_array(NULL) == NULL);
  check(!js_cadeia("\"x\"", NULL, 8));
  check(!js_texto("{}", NULL, NULL, NULL, 8));
  check(!js_tem(NULL, NULL, "x") && js_num(NULL, NULL, "n", -1) == -1);
}

int main(void) {
  document_fixtures();
  every_unicode_scalar();
  malformed_bytes();
  protected_ranges();
  timestamps_and_numeric_limits();
  printf("Rust JSON reader: %zu comparisons passed (all Unicode scalars, approved prefix fix, protected ranges)\n", comparisons);
  return 0;
}
