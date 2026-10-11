#ifndef NV_BUSCANORM_H
#define NV_BUSCANORM_H
#include <stddef.h>

// Local search normalization, independent of screens (#176). Provider search
// receives the original typed term. Fold ASCII case, Latin-1/Extended-A and
// Romanian accents to ASCII; lowercase supported Cyrillic and fold ё to е.
// Other scripts pass through. Invalid bytes become spaces; Rust also rejects
// overlongs, surrogates and values above U+10FFFF (approved migration fix).
// Capacity includes NUL; output stops before a complete character. Null input
// means empty; null/zero-capacity output is ignored. Exact in-place copying
// is supported; partially overlapping buffers are unsupported.
void busca_normalizar(const char *input, char *output, size_t capacity);

// Count non-continuation bytes (characters for valid UTF-8). Null means zero.
int busca_codepoints(const char *input);

// Delete a complete final character in place; return the new byte length.
// Buffer storage includes the NUL at length. Backspace never splits UTF-8.
size_t busca_apagar_ultimo(char *buffer, size_t length);

#endif
