#ifndef NV_RUST_CORE_H
#define NV_RUST_CORE_H
#include <stddef.h>
#include <stdint.h>

#define NUVIO_RUST_ABI_VERSION 1u
#define NUVIO_RUST_MEMORY_OK 1u
#define NUVIO_RUST_THREAD_OK 2u
#define NUVIO_RUST_FILE_OK 4u
#define NUVIO_RUST_CHECKS_OK 7u
#define NUVIO_RUST_CHECKSUM UINT64_C(522240)

typedef struct {
  uint32_t abi_version;
  uint32_t checks;
  uint64_t checksum;
} NuvioRustProbe;

uint32_t nuvio_rust_core_abi_version(void);
/* Caller owns both arguments throughout the call. No allocations cross the ABI.
 * directory: NUL-terminated UTF-8 writable directory, never retained.
 * output: aligned storage, written for valid inputs even if a check fails.
 * Returns 0 on success, 1 for invalid input, 2 for thread failure, 3 for file I/O.
 * Rust panics abort instead of unwinding into C. */
int32_t nuvio_rust_core_probe(const char *directory, NuvioRustProbe *output);

/* Add-on URL ABI: caller-owned byte strings, not necessarily UTF-8.
 * Readable inputs must be NUL-terminated; null means empty. Writable outputs
 * must have capacity bytes and must not overlap inputs. Null/zero-capacity
 * output is accepted without writing. No allocation or pointer is retained.
 * The adapter in addonurl.h preserves existing C function names/variadic calls. */
int32_t nuvio_rust_addon_url_fits(size_t length);
int32_t nuvio_rust_addon_request_fits(int32_t length, size_t capacity);
/* Empty output on overflow. Normalization preserves queries verbatim. */
void nuvio_rust_addon_base(const char *input, char *output, size_t capacity);
/* 1 = encoded ID fits, 0 = empty output or unusable output buffer. */
int32_t nuvio_rust_addon_id(char *output, size_t capacity, const char *input);
/* snprintf-style required length, -1 for format failure/unusable output.
 * formatted_length is vsnprintf's result for a 3072-byte path buffer.
 * path may be null only when formatting failed or overflowed that buffer;
 * overflow returns trimmed base length + formatted_length without the query.
 * A successful format may contain embedded NUL: only its visible prefix joins.
 * Truncated requests are emptied. Never send them to a provider. */
int32_t nuvio_rust_addon_request(char *output, size_t capacity, const char *base,
                               const char *path, int32_t formatted_length);

/* Text helpers: no allocation, I/O, threads or retained pointers.
 * Strings are NUL-terminated bytes; null input means empty. Outputs are
 * caller-owned. Null/zero-capacity output writes nothing. Capacity includes
 * the NUL. Cleanup and search truncate before a complete output character.
 * Cleanup inputs/outputs must not overlap. Search permits exact in-place
 * normalization; partial overlap is unsupported. Invalid UTF-8 bytes drop
 * during cleanup and become spaces during search. Valid content retains the
 * original font, accent, case, flag/language and separator rules. */
char nuvio_rust_fold_stylized_character(unsigned long codepoint);
size_t nuvio_rust_text_cleanup(const char *input, char *output, size_t capacity,
                              int32_t flags);
/* In-place, readable/writable NUL-terminated buffer; never grows. Null = 0. */
size_t nuvio_rust_text_trim_separators(char *buffer);
void nuvio_rust_search_normalize(const char *input, char *output, size_t capacity);
/* Counts non-continuation bytes, including for malformed input. Count must
 * fit int32_t; null = 0. */
int32_t nuvio_rust_search_codepoints(const char *input);
/* buffer is writable through length inclusive; length is its current byte
 * length. Delete the final character in place; return the new length. Null = 0. */
size_t nuvio_rust_search_delete_last(char *buffer, size_t length);

/* Tolerant JSON reader, matching js.h. No allocation or pointer retention.
 * start/end: readable range in one allocation; end must be >= start. A null
 * end means NUL-terminated input. Explicit ranges need no NUL terminator.
 * key and standalone strings are NUL-terminated bytes. Null inputs fail.
 * Output owns capacity bytes, including NUL, and must not overlap inputs.
 * Null/zero-capacity output writes nothing. Returned pointers borrow input.
 * General text/raw failures preserve output; root text clears it on valid
 * non-null arguments. Empty decoded strings write NUL and return 0. Decoded
 * text retains only the fitting prefix; raw JSON must fit in full.
 * Native strtod/sscanf/timegm preserve GNU runtime conversion/date behavior. */
#define NUVIO_RUST_JSON_KEEP_LINES 1u
#define NUVIO_RUST_JSON_ROOT_ONLY 2u
const char *nuvio_rust_json_end(const char *input);
int32_t nuvio_rust_json_has(const char *start, const char *end, const char *key);
int32_t nuvio_rust_json_text(const char *start, const char *end, const char *key,
                           char *output, size_t capacity, uint32_t flags);
int32_t nuvio_rust_json_string(const char *input, char *output, size_t capacity);
double nuvio_rust_json_number(const char *start, const char *end, const char *key,
                             double fallback);
const char *nuvio_rust_json_array(const char *start, const char *end, const char *key);
const char *nuvio_rust_json_next(const char *previous_end);
const char *nuvio_rust_json_root_array(const char *body);
int32_t nuvio_rust_json_raw(const char *start, const char *end, const char *key,
                          char *output, size_t capacity);
int64_t nuvio_rust_json_iso_ms(const char *input);

/* JSON writer: initialized caller-owned Jsw storage with the layout in jsw.h.
 * init accepts uninitialized storage that owns no live buffer; free releases
 * the buffer and resets storage, and may be repeated. Other calls require an
 * initialized context exclusively accessible for mutation. Null contexts are
 * accepted without writing; finish returns null for absent/failed output.
 * Native realloc/free own the output buffer; Rust exports no Rust allocation.
 * Inputs are NUL-terminated bytes, disjoint from the context and its buffer.
 * Returned output is borrowed until mutation/free. A caller may wipe its
 * bytes before free, matching the existing password-body cleanup contract.
 * String controls are escaped; non-ASCII/raw JSON bytes are preserved.
 * Maximum depth is 16; error is sticky until free/init. No JSON validation.
 * No exceptions/unwinding cross the ABI; no input pointer is retained. */
struct Jsw;
void nuvio_rust_json_writer_init(struct Jsw *writer);
void nuvio_rust_json_writer_free(struct Jsw *writer);
const char *nuvio_rust_json_writer_finish(const struct Jsw *writer);
void nuvio_rust_json_writer_open(struct Jsw *writer, uint8_t delimiter);
void nuvio_rust_json_writer_close(struct Jsw *writer, uint8_t delimiter);
void nuvio_rust_json_writer_key(struct Jsw *writer, const char *key);
void nuvio_rust_json_writer_string(struct Jsw *writer, const char *input);
void nuvio_rust_json_writer_integer(struct Jsw *writer, int64_t value);
void nuvio_rust_json_writer_boolean(struct Jsw *writer, int32_t value);
void nuvio_rust_json_writer_null(struct Jsw *writer);
void nuvio_rust_json_writer_raw(struct Jsw *writer, const char *input);
#endif
