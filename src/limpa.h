#ifndef NV_LIMPA_H
#define NV_LIMPA_H
#include <stddef.h>

// Provider text -> source-list text (#144), independent of SDL_ttf.
// Preserve the bundled-font rules: visible BMP scripts pass through for
// fallback fonts; styled letters fold to ASCII, flags become language/country
// codes, and supported check/cross/star/arrow symbols retain mapped glyphs.
// Decoration between words becomes " · "; controls, invalid UTF-8, variation
// selectors and unsupported non-BMP characters disappear.
// SINGLE_LINE replaces line breaks with separators; otherwise retain breaks.
// Output truncates before a complete character/prefix; return its byte length
// excluding NUL. Null input means empty; null/zero-capacity output writes
// nothing. Input and output must not overlap. The C ABI name/flag stay stable.
#define NV_LIMPA_UMA_LINHA 1
size_t nv_limpar_texto(const char *input, char *output, size_t capacity, int flags);

// Tidy ONE already-cleaned line in place. Collapse spaces; discard leading/
// trailing separator-only words (| - / , • ·) and keep the last of doubled
// interior separators. Trim separators attached to the first/last word.
// Preserve interior hyphens (WEB-DL). Return the new byte length.
size_t nv_aparar_separadores(char *buffer);

#endif
