// Shared tolerant JSON reading for known provider/account formats.
// This is a byte scanner, not a complete JSON validator. Missing fields and
// unexpected types leave fallback decisions to the caller. Existing C entry
// points stay available while their Rust Test implementation moves to Rust.
#ifndef NV_JS_H
#define NV_JS_H
#include <stddef.h>

// End of the object/array starting at input, respecting quotes and escapes.
// Input is NUL-terminated; incomplete containers return the NUL position.
const char *js_fim(const char *input);

// Presence of a key in [start,end), including null/malformed values.
// For every ranged reader, a null end means NUL-terminated input; explicit
// ranges need no terminator. Returned pointers borrow the input allocation.
int js_tem(const char *start, const char *end, const char *key);

// Decode a string field, including Unicode escapes and surrogate pairs.
// Output capacity includes NUL. Missing/non-string/incomplete values preserve
// output. Empty decoded strings write NUL and return 0. Nonempty prefixes
// return 1. Rust Test stops before the first character that will not fit;
// the retained C implementation may skip oversized escaped characters.
// Input, key and writable output buffers must not overlap.
int js_texto(const char *start, const char *end, const char *key,
             char *output, size_t capacity);
// Same reader, keeping escaped newlines, dropping CR and replacing tabs.
int js_texto_linhas(const char *start, const char *end, const char *key,
                    char *output, size_t capacity);

// Decode a standalone NUL-terminated JSON string starting at its opening quote.
int js_cadeia(const char *input, char *output, size_t capacity);

// Finite number, accepting quoted ratings and usable nested occurrences:
// {"currentTime":{"currentTime":8580}} must return 8580, not the fallback.
double js_num(const char *start, const char *end, const char *key, double fallback);

// First object/string in a named array; NULL for empty/other element types.
const char *js_array(const char *start, const char *end, const char *key);
// Next object/string after the previous element's end; NUL-terminated input.
const char *js_prox(const char *previous_end);
// First object/string in a NUL-terminated root array, such as a Supabase RPC.
const char *js_raiz_array(const char *body);

// Copy a complete raw JSON value, including braces/brackets/string quotes.
// Credential forwarding needs this to retain fields unknown to this app.
// Returns 1 only if the entire value fits; failure preserves output.
int js_bruto(const char *start, const char *end, const char *key,
             char *output, size_t capacity);

// Established timestamp reader: UTC milliseconds since the Unix epoch.
// Accepts dates, partial times and fractional seconds; returns 0 on failure.
// Native calendar normalization/range and permissive parsing are preserved.
// Reads at most three fractional digits. Time-zone suffixes are ignored:
// account/Trakt callers provide UTC; this is not a general ISO-8601 parser.
long long js_ms_iso(const char *input);

// String field at depth 1, ignoring nested names/IDs. Clears output before
// lookup for non-null input/key/output and positive capacity. Other text
// decoding/return rules match js_texto. Prevents a catalog ID being mistaken
// for a manifest ID, or a nested genre being mistaken for a TMDB title.
int js_texto_raiz(const char *body, const char *key, char *output, size_t capacity);
// Same lookup at the first '{' in a bounded object, such as catalogs[].
int js_texto_raiz_em(const char *start, const char *end, const char *key,
                     char *output, size_t capacity);

#endif
