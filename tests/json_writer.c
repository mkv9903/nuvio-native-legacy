// JSON writer regressions, real ABI parity and guarded native allocation faults.
#include "jsw.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void reference_writer_init(Jsw *);
void reference_writer_free(Jsw *);
const char *reference_writer_finish(const Jsw *);
void reference_writer_object_open(Jsw *);
void reference_writer_object_close(Jsw *);
void reference_writer_array_open(Jsw *);
void reference_writer_array_close(Jsw *);
void reference_writer_key(Jsw *, const char *);
void reference_writer_string(Jsw *, const char *);
void reference_writer_integer(Jsw *, long long);
void reference_writer_boolean(Jsw *, int);
void reference_writer_null(Jsw *);
void reference_writer_raw(Jsw *, const char *);
void reference_writer_key_string(Jsw *, const char *, const char *);
void reference_writer_key_integer(Jsw *, const char *, long long);
void reference_writer_key_boolean(Jsw *, const char *, int);

static size_t comparisons;
static void check(int condition) {
  comparisons++;
  if (!condition) { fprintf(stderr, "JSON writer comparison %zu failed\n", comparisons); abort(); }
}

// Linker wrapping reaches the actual allocator calls in C and Rust. The
// extra suffix checks Rust writes as well as C, which alone has ASan coverage.
#define GUARD_SIZE 16
typedef struct { void *pointer; size_t capacity; } Allocation;
static Allocation allocations[32];
static long allocation_budget = -1;
static size_t releases;
void *__real_realloc(void *, size_t);
void __real_free(void *);

static int allocation_slot(void *pointer) {
  for (int i = 0; i < 32; i++) if (allocations[i].pointer == pointer) return i;
  return -1;
}
static void check_guard(int slot) {
  assert(slot >= 0 && allocations[slot].pointer);
  const unsigned char *guard = (unsigned char *)allocations[slot].pointer + allocations[slot].capacity;
  for (size_t i = 0; i < GUARD_SIZE; i++) check(guard[i] == 0xa5);
}
void *__wrap_realloc(void *pointer, size_t capacity) {
  int slot = allocation_slot(pointer);
  assert(slot >= 0);
  if (pointer) check_guard(slot);
  if (allocation_budget == 0) return NULL;
  if (allocation_budget > 0) allocation_budget--;
  assert(capacity <= SIZE_MAX - GUARD_SIZE);
  void *result = __real_realloc(pointer, capacity + GUARD_SIZE);
  if (!result) return NULL;
  allocations[slot] = (Allocation){result, capacity};
  memset((unsigned char *)result + capacity, 0xa5, GUARD_SIZE);
  return result;
}
void __wrap_free(void *pointer) {
  if (pointer) {
    int slot = allocation_slot(pointer);
    if (slot >= 0) {
      check_guard(slot);
      allocations[slot] = (Allocation){0};
      releases++;
    }
  }
  __real_free(pointer);
}

typedef struct { Jsw actual, reference; } Pair;
static void compare_pair(const Pair *pair) {
  const Jsw *actual = &pair->actual, *reference = &pair->reference;
  check(actual->n == reference->n && actual->cap == reference->cap);
  check(actual->erro == reference->erro && actual->prof == reference->prof);
  check(!memcmp(actual->primeiro, reference->primeiro, JSW_PROF));
  check((actual->p == NULL) == (reference->p == NULL));
  check((jsw_texto_final(actual) == NULL) == (reference_writer_finish(reference) == NULL));
  if (actual->p) {
    check(actual->n < actual->cap && actual->p[actual->n] == 0);
    check(!memcmp(actual->p, reference->p, actual->n + 1));
    check_guard(allocation_slot(actual->p));
    check_guard(allocation_slot(reference->p));
  }
}
static void init_pair(Pair *pair) {
  memset(pair, 0x55, sizeof *pair);
  jsw_iniciar(&pair->actual); reference_writer_init(&pair->reference);
  compare_pair(pair);
}
static void free_pair(Pair *pair) {
  jsw_livre(&pair->actual); reference_writer_free(&pair->reference);
  compare_pair(pair);
}

enum Operation { OBJECT_OPEN, OBJECT_CLOSE, ARRAY_OPEN, ARRAY_CLOSE, KEY,
                 STRING, INTEGER, BOOLEAN, NULL_VALUE, RAW, KEY_STRING, KEY_INTEGER, KEY_BOOLEAN };
static void apply_one(Jsw *writer, int reference, enum Operation operation,
                      const char *key, const char *text, long long number) {
  switch (operation) {
    case OBJECT_OPEN: (reference ? reference_writer_object_open : jsw_obj_ini)(writer); break;
    case OBJECT_CLOSE: (reference ? reference_writer_object_close : jsw_obj_fim)(writer); break;
    case ARRAY_OPEN: (reference ? reference_writer_array_open : jsw_arr_ini)(writer); break;
    case ARRAY_CLOSE: (reference ? reference_writer_array_close : jsw_arr_fim)(writer); break;
    case KEY: (reference ? reference_writer_key : jsw_chave)(writer, key); break;
    case STRING: (reference ? reference_writer_string : jsw_str)(writer, text); break;
    case INTEGER: (reference ? reference_writer_integer : jsw_int)(writer, number); break;
    case BOOLEAN: (reference ? reference_writer_boolean : jsw_bool)(writer, (int)number); break;
    case NULL_VALUE: (reference ? reference_writer_null : jsw_nulo)(writer); break;
    case RAW: (reference ? reference_writer_raw : jsw_bruto)(writer, text); break;
    case KEY_STRING: (reference ? reference_writer_key_string : jsw_cs)(writer, key, text); break;
    case KEY_INTEGER: (reference ? reference_writer_key_integer : jsw_ci)(writer, key, number); break;
    case KEY_BOOLEAN: (reference ? reference_writer_key_boolean : jsw_cb)(writer, key, (int)number); break;
  }
}
static void apply(Pair *pair, enum Operation operation, const char *key, const char *text, long long number) {
  apply_one(&pair->actual, 0, operation, key, text, number);
  apply_one(&pair->reference, 1, operation, key, text, number);
  compare_pair(pair);
}
static void expected(const Pair *pair, const char *output) {
  check(jsw_texto_final(&pair->actual) && !strcmp(jsw_texto_final(&pair->actual), output));
}

static void serialization_regressions(void) {
  Pair pair; init_pair(&pair);
  apply(&pair, OBJECT_OPEN, NULL, NULL, 0);
  apply(&pair, KEY_INTEGER, "a", NULL, 1);
  apply(&pair, OBJECT_CLOSE, NULL, NULL, 0);
  expected(&pair, "{\"a\":1}"); // The historical double-separator regression.
  free_pair(&pair); free_pair(&pair);

  init_pair(&pair);
  apply(&pair, OBJECT_OPEN, NULL, NULL, 0);
  apply(&pair, KEY_STRING, "title", "日é😀\"\\\n\r\t\x01\x08\x0c", 0);
  apply(&pair, KEY, "items", NULL, 0);
  apply(&pair, ARRAY_OPEN, NULL, NULL, 0);
  apply(&pair, INTEGER, NULL, NULL, LLONG_MIN);
  apply(&pair, INTEGER, NULL, NULL, LLONG_MAX);
  apply(&pair, BOOLEAN, NULL, NULL, -1);
  apply(&pair, BOOLEAN, NULL, NULL, 0);
  apply(&pair, STRING, NULL, NULL, 0);
  apply(&pair, STRING, NULL, "", 0);
  apply(&pair, RAW, NULL, "{\"unknown\":true}", 0);
  apply(&pair, RAW, NULL, "", 0);
  apply(&pair, RAW, NULL, NULL, 0);
  apply(&pair, ARRAY_CLOSE, NULL, NULL, 0);
  apply(&pair, KEY_STRING, NULL, NULL, 0);
  apply(&pair, OBJECT_CLOSE, NULL, NULL, 0);
  expected(&pair, "{\"title\":\"日é😀\\\"\\\\\\n\\r\\t\\u0001\\u0008\\u000c\",\"items\":[-9223372036854775808,9223372036854775807,true,false,null,\"\",{\"unknown\":true},null,null],\"\":null}");
  free_pair(&pair);

  for (unsigned byte = 1; byte < 256; byte++) {
    char input[2] = {(char)byte, 0};
    init_pair(&pair);
    apply(&pair, OBJECT_OPEN, NULL, NULL, 0);
    apply(&pair, KEY_STRING, input, input, 0);
    apply(&pair, KEY_BOOLEAN, "boolean", NULL, INT_MIN);
    apply(&pair, OBJECT_CLOSE, NULL, NULL, 0);
    free_pair(&pair);
  }
  init_pair(&pair);
  apply(&pair, STRING, NULL, "A\0ignored", 0);
  expected(&pair, "\"A\"");
  free_pair(&pair);
}

static uint64_t random_state = UINT64_C(0x8631deac);
static uint64_t next_random(void) {
  random_state ^= random_state << 13; random_state ^= random_state >> 7;
  random_state ^= random_state << 17; return random_state;
}
static void operation_sequences(void) {
  const char *texts[] = {NULL, "", "plain", "日é😀", "a\n\r\t\b\f\\\"", "{\"extra\":[1,null,true]}", "invalid raw JSON", "\xc0\x80\xff"};
  const char *keys[] = {NULL, "", "id", "name", "quote\"\\", "日é😀"};
  for (size_t iteration = 0; iteration < 500; iteration++) {
    Pair pair; init_pair(&pair);
    for (size_t step = 0; step < 80; step++) {
      uint64_t bits = next_random(); long long number;
      memcpy(&number, &bits, sizeof number);
      apply(&pair, (enum Operation)(next_random() % 13), keys[next_random() % 6], texts[next_random() % 8], number);
    }
    free_pair(&pair);
  }
  Pair pair; init_pair(&pair);
  apply(&pair, ARRAY_OPEN, NULL, NULL, 0);
  for (size_t i = 0; i < 2000; i++) {
    uint64_t bits = next_random(); long long value; memcpy(&value, &bits, sizeof value);
    apply(&pair, INTEGER, NULL, NULL, value);
  }
  apply(&pair, ARRAY_CLOSE, NULL, NULL, 0);
  free_pair(&pair);
}

static void growth_and_failures(void) {
  char large[16385]; memset(large, 'x', sizeof large - 1); large[sizeof large - 1] = 0;
  for (size_t length = 0; length <= sizeof large - 1; length++) {
    if (length > 270 && length != 510 && length != 511 && length != 512 &&
        length != 1023 && length != 4096 && length != sizeof large - 1) continue;
    char saved = large[length]; large[length] = 0;
    Pair pair; init_pair(&pair); apply(&pair, STRING, NULL, large, 0); free_pair(&pair);
    large[length] = saved;
  }
  Pair pair; init_pair(&pair);
  for (int depth = 0; depth <= JSW_PROF; depth++) apply(&pair, ARRAY_OPEN, NULL, NULL, 0);
  check(pair.actual.erro && pair.actual.prof == JSW_PROF && !jsw_texto_final(&pair.actual));
  size_t length = pair.actual.n;
  for (int depth = 0; depth <= JSW_PROF; depth++) {
    apply(&pair, OBJECT_CLOSE, NULL, NULL, 0);
    apply(&pair, KEY_STRING, "after-error", "ignored", 0);
  }
  check(pair.actual.n == length);
  apply(&pair, OBJECT_OPEN, NULL, NULL, 0);
  free_pair(&pair);

  for (long budget = 0; budget <= 8; budget++) {
    init_pair(&pair);
    allocation_budget = budget;
    apply_one(&pair.actual, 0, OBJECT_OPEN, NULL, NULL, 0);
    apply_one(&pair.actual, 0, KEY_STRING, "payload", large, 0);
    apply_one(&pair.actual, 0, OBJECT_CLOSE, NULL, NULL, 0);
    allocation_budget = budget;
    apply_one(&pair.reference, 1, OBJECT_OPEN, NULL, NULL, 0);
    apply_one(&pair.reference, 1, KEY_STRING, "payload", large, 0);
    apply_one(&pair.reference, 1, OBJECT_CLOSE, NULL, NULL, 0);
    allocation_budget = -1;
    compare_pair(&pair);
    if (budget < 8) check(!jsw_texto_final(&pair.actual));
    free_pair(&pair);
    apply(&pair, INTEGER, NULL, NULL, 42); expected(&pair, "42"); free_pair(&pair);
  }
}

static void native_ownership(void) {
  Pair pair; init_pair(&pair);
  apply(&pair, STRING, NULL, "synthetic-password-body", 0);
  char *body = (char *)jsw_texto_final(&pair.actual);
  memset(body, 0, pair.actual.n); // Existing session.c password cleanup.
  memset(pair.reference.p, 0, pair.reference.n);
  compare_pair(&pair);
  size_t before = releases; free_pair(&pair); check(releases == before + 2);
  // C can release the native buffer; no Rust allocator metadata is required.
  Jsw writer; jsw_iniciar(&writer); jsw_int(&writer, 7);
  check(writer.p && !strcmp(writer.p, "7"));
  free(writer.p); writer.p = NULL; writer.n = writer.cap = 0;
  jsw_livre(&writer);
#ifdef NV_RUST_CORE
  jsw_iniciar(NULL); jsw_livre(NULL); jsw_str(NULL, NULL);
  check(jsw_texto_final(NULL) == NULL);
#endif
  for (size_t i = 0; i < 32; i++) check(allocations[i].pointer == NULL);
}

int main(void) {
  _Static_assert(sizeof(long long) == sizeof(uint64_t), "64-bit integer fixtures");
  serialization_regressions(); operation_sequences(); growth_and_failures(); native_ownership();
  printf("JSON writer: %zu comparisons passed (bytes, state, guards, allocation failures, native ownership)\n", comparisons);
  return 0;
}
