/* Recent searches are private to the selected Rust account/profile.
 * Queries are complete borrowed UTF-8 strings, valid until a UI snapshot refresh.
 * Only confirmed searches are recorded; rendering never performs disk I/O. */
#ifndef NV_SEARCH_HISTORY_H
#define NV_SEARCH_HISTORY_H
#include "rust_search_history.h"
#define SEARCH_HISTORY_MAX 10
void search_history_init(RustAccount *account, const char *directory);
void search_history_shutdown(void);
void search_history_step(void);
uint64_t search_history_revision(void);
uint64_t search_history_snapshot_revision(void);
int search_history_count(void);
const char *search_history_query(int index);
int search_history_record(const char *query);
void search_history_remove(int index);
void search_history_clear(void);
void search_history_logout(void);
#endif
