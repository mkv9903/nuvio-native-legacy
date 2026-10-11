// Shared JSON writing with automatic separators and escaped string values.
// Rust Test delegates emission to Rust; existing C entry points/layout remain.
#ifndef NV_JSW_H
#define NV_JSW_H
#include <stddef.h>

#define JSW_PROF 16   // Maximum nesting depth; retained compatibility name.

// Caller-owned context. Legacy field names retain layout compatibility with
// existing C callers; Rust uses buffer/length/capacity/error/depth/first.
// Initialize before use, free before reinitializing a live context, and never
// copy a live context by value: its native buffer has a single owner.
typedef struct Jsw {
  char  *p;
  size_t n, cap;
  int    erro;                 // Sticky error: no further output is emitted.
  int    prof;                 // Depth bookkeeping retains the legacy rules.
  char   primeiro[JSW_PROF];   // Whether the current level has no element yet.
} Jsw;

void jsw_iniciar(Jsw *writer);
void jsw_livre(Jsw *writer);   // Free/reset; repeating this is valid.

// Borrowed NUL-terminated output, or NULL for absent/failed output. Mutation
// and free invalidate the result. May be wiped before free for password bodies.
const char *jsw_texto_final(const Jsw *writer);

void jsw_obj_ini(Jsw *writer);
void jsw_obj_fim(Jsw *writer);
void jsw_arr_ini(Jsw *writer);
void jsw_arr_fim(Jsw *writer);

// Write a key, with null meaning an empty key. The next operation writes its
// value without a second separator. This API does not validate JSON structure.
void jsw_chave(Jsw *writer, const char *key);

// Inputs are NUL-terminated bytes disjoint from the context/output buffer.
// Strings escape controls, quotes and backslashes; other bytes pass through.
void jsw_str(Jsw *writer, const char *input);  // Null input emits JSON null.
void jsw_int(Jsw *writer, long long value);
void jsw_bool(Jsw *writer, int value);        // Nonzero means true.
void jsw_nulo(Jsw *writer);

// Forward already-serialized JSON, including unknown credential fields.
// No validation or escaping; null/empty input emits JSON null.
void jsw_bruto(Jsw *writer, const char *json);

// Key/value convenience operations.
void jsw_cs(Jsw *writer, const char *key, const char *value);
void jsw_ci(Jsw *writer, const char *key, long long value);
void jsw_cb(Jsw *writer, const char *key, int value);

#endif
