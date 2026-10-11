/* Compare the actual Rust ABI/adapter with the C reference using synthetic
 * inputs, capacity boundaries, every non-NUL byte and reproducible randomness.
 * Never print configured URLs or IDs, including when a comparison fails. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "addonurl.h"

#ifndef NV_RUST_CORE
#error This test must call the Rust implementation
#endif

void reference_addon_base(const char *, char *, size_t);
int reference_addon_id(char *, size_t, const char *);
int reference_addon_request(char *, size_t, const char *, const char *);
int reference_addon_formatted(char *, size_t, const char *, const char *, int, int);

enum { CAPACITY = 8192, GUARD = 32 };
static unsigned char c_buffer[CAPACITY + GUARD * 2], rust_buffer[CAPACITY + GUARD * 2];
static unsigned comparisons;

static void require(int condition, const char *check) {
  if (condition) return;
  fprintf(stderr, "FAIL: %s at comparison %u\n", check, comparisons);
  exit(1);
}

static void reset(size_t capacity) {
  require(capacity <= CAPACITY, "test buffer capacity");
  memset(c_buffer, 0xa5, sizeof c_buffer);
  memset(rust_buffer, 0xa5, sizeof rust_buffer);
}

static char *c_output(void) { return (char *)c_buffer + GUARD; }
static char *rust_output(void) { return (char *)rust_buffer + GUARD; }

static void check_output(size_t capacity) {
  comparisons++;
  for (size_t i = 0; i < sizeof c_buffer; i++) {
    if (i >= GUARD && i < GUARD + capacity) continue;
    require(c_buffer[i] == 0xa5, "C output guard");
    require(rust_buffer[i] == 0xa5, "Rust output guard");
  }
  if (capacity) {
    require(memchr(c_output(), 0, capacity) != NULL, "C output terminator");
    require(memchr(rust_output(), 0, capacity) != NULL, "Rust output terminator");
    require(strcmp(c_output(), rust_output()) == 0, "C/Rust output parity");
  }
}

static void check_base(const char *input, size_t capacity) {
  reset(capacity);
  reference_addon_base(input, c_output(), capacity);
  nv_addon_base(input, rust_output(), capacity);
  check_output(capacity);
}

static void check_id(const char *input, size_t capacity) {
  reset(capacity);
  int expected = reference_addon_id(c_output(), capacity, input);
  int actual = nv_addon_id(rust_output(), capacity, input);
  require(actual == expected, "encoded ID return value");
  check_output(capacity);
}

static void check_request(const char *base, const char *path, size_t capacity) {
  reset(capacity);
  int expected = reference_addon_request(c_output(), capacity, base, path);
  int actual = nv_addon_url(rust_output(), capacity, base, "%s", path);
  require(actual == expected, "request return value");
  check_output(capacity);
}

static void check_formatted(const char *base, int separator, size_t capacity) {
  reset(capacity);
  int expected = reference_addon_formatted(c_output(), capacity, base, "tt1", separator, 12);
  int actual = nv_addon_url(rust_output(), capacity, base, "/stream/%s%c%d.json", "tt1", separator, 12);
  require(actual == expected, "variadic format return value");
  check_output(capacity);
}

static uint32_t random_state = UINT32_C(0x72ad41e3);
static uint32_t next_random(void) {
  random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
  return random_state;
}

static void random_bytes(char *output, size_t length) {
  for (size_t i = 0; i < length; i++) output[i] = (char)(1 + next_random() % 255);
  output[length] = 0;
}

int main(void) {
  const char *cases[] = {
    NULL, "", " \t\n\v\f\r", "/", "////?", "/manifest.json", "x/manifest.json",
    "\vStReMiO://s.test/config///MANIFEST.JSON///?key=a%2Fb&&x=1\f",
    "  https://x.test/cfg//manifest.json///?  ", "https://x.test/cfg?key=one?two#three",
    "stremio://", "stremio://?x=1", "?x=1", "https://x.test/manifest.jsonx",
    "https://x.test/caf\xc3\xa9/manifest.json?key=%FF", "\xa0https://x.test/\xa0",
    "tt0111161:1:2", "iptv:Canal Um/HD#1?a%\xc3\xa7", "-._~:@!$&'()*+,;=",
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    for (size_t capacity = 0; capacity < 200; capacity++) {
      check_base(cases[i], capacity);
      check_id(cases[i], capacity);
      check_request(cases[i], "/stream/movie/tt1.json", capacity);
      check_formatted(cases[i], ':', capacity);
      check_formatted(cases[i], 0, capacity);
    }
  }
  // Byte-oriented encoding must preserve all non-ASCII bytes via %HH.
  char every_byte[256];
  for (unsigned i = 1; i <= 255; i++) every_byte[i - 1] = (char)i;
  every_byte[255] = 0;
  for (size_t capacity = 0; capacity <= 255 * 3 + 1; capacity++) check_id(every_byte, capacity);

  // Exercise both sides of the base and formatted-path limits, including the
  // old early-return rule which omits the base query for an oversized path.
  char base[2200], path[NV_ADDON_PEDIDO_MAX + 128];
  for (size_t length = 2046; length <= 2049; length++) {
    memset(base, 'b', length); base[length] = 0;
    check_base(base, 2048);
    require(nuvio_rust_addon_url_fits(length) == (length < NV_ADDON_URL_MAX), "base length rule");
  }
  for (size_t length = NV_ADDON_PEDIDO_MAX - 1; length <= NV_ADDON_PEDIDO_MAX + 1; length++) {
    memset(path, 'p', length); path[length] = 0;
    const size_t capacities[] = {0, 1, 32, NV_ADDON_PEDIDO_MAX, CAPACITY};
    for (size_t i = 0; i < sizeof capacities / sizeof capacities[0]; i++)
      check_request("https://x.test/base///?key=hidden", path, capacities[i]);
    require(nv_addon_url(NULL, 0, "https://x.test/base///?key=hidden", "%s", path)
            == reference_addon_request(NULL, 0, "https://x.test/base///?key=hidden", path),
            "null output for oversized path");
  }

  for (unsigned round = 0; round < 1000; round++) {
    size_t base_length = next_random() % 120;
    size_t path_length = next_random() % 160;
    if (round % 25 == 0) base_length += 2000;
    if (round % 40 == 0) path_length = NV_ADDON_PEDIDO_MAX + next_random() % 100;
    random_bytes(base, base_length);
    random_bytes(path, path_length);
    const size_t capacities[] = {0, 1, 2, 20, base_length, base_length + 1,
                                base_length * 3 + 1, base_length + path_length + 1, CAPACITY};
    for (size_t i = 0; i < sizeof capacities / sizeof capacities[0]; i++) {
      check_base(base, capacities[i]);
      check_id(base, capacities[i]);
      check_request(base, path, capacities[i]);
    }
  }

  // Nulls and C formatting failure are part of the explicit bridge contract.
  nv_addon_base("https://x.test", NULL, 30);
  require(nv_addon_id(NULL, 30, "tt1") == 0, "null ID output");
  require(nv_addon_url(NULL, 30, NULL, "/manifest.json") == -1, "null request output");
  reset(32);
  require(nuvio_rust_addon_request(rust_output(), 32, "https://x.test", NULL, -1) == -1,
          "negative format result");
  require(rust_output()[0] == 0, "failed format output empty");
  require(nuvio_rust_addon_request(rust_output(), 32, NULL, NULL, 0) == -1,
          "missing successful path");
  require(rust_output()[0] == 0, "missing path output empty");
  require(nuvio_rust_addon_request_fits(-1, 32) == 0, "negative request rejected");
  require(nuvio_rust_addon_request_fits(0, 0) == 0, "zero-capacity request rejected");
  require(nuvio_rust_addon_request_fits(31, 32) == 1, "exact request fit");
  require(nuvio_rust_addon_request_fits(32, 32) == 0, "NUL needs capacity");
  require(nuvio_rust_addon_request_fits(INT_MAX, (size_t)INT_MAX + 1) == 1, "size_t capacity ABI");
  // Exercise the English rejection logs; the shell verifies URL privacy.
  require(nv_addon_url_cabe(NULL, NV_ADDON_URL_MAX) == 0, "URL rejection");
  require(nv_addon_pedido_coube("Sample", 32, 32) == 0, "request rejection");
  require(nv_addon_pedido_coube("Sample", -1, 0) == 0, "zero-capacity diagnostic");
  printf("rust_addon_url: %u C/Rust parity comparisons passed\n", comparisons);
  return 0;
}
