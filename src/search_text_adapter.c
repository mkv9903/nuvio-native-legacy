/* Native text-field projections. Rust owns decoding, normalization and edits. */
#include "search_text.h"
extern void nuvio_search_text_normalize(const char *, char *, size_t);
extern int nuvio_search_text_count(const char *);
extern size_t nuvio_search_text_delete_last(char *, size_t);
void search_normalize(const char *text, char *output, size_t capacity) {
  nuvio_search_text_normalize(text, output, capacity);
}
int search_character_count(const char *text) { return nuvio_search_text_count(text); }
size_t search_delete_last_character(char *text, size_t length) {
  return nuvio_search_text_delete_last(text, length);
}
