#ifndef NV_SEARCH_TEXT_H
#define NV_SEARCH_TEXT_H
#include <stddef.h>
/* Local matching only. Provider requests receive the original query text.
 * Output uses a complete UTF-8 prefix; capacity includes its terminating NUL. */
void search_normalize(const char *text, char *output, size_t capacity);
int search_character_count(const char *text);
size_t search_delete_last_character(char *text, size_t length);
#endif
