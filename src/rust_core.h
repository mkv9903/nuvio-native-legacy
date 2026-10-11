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
#endif
