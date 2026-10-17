#ifndef NV_RUST_SEARCH_HISTORY_H
#define NV_RUST_SEARCH_HISTORY_H
#include "rust_account.h"
#include <stddef.h>
#include <stdint.h>
typedef struct RustSearchHistory RustSearchHistory;
typedef struct RustSearchHistoryView RustSearchHistoryView;
RustSearchHistory *nuvio_search_history_create(const RustAccount *account, const char *directory);
void nuvio_search_history_destroy(RustSearchHistory *history);
void nuvio_search_history_step(const RustSearchHistory *history);
uint64_t nuvio_search_history_revision(const RustSearchHistory *history);
RustSearchHistoryView *nuvio_search_history_snapshot(const RustSearchHistory *history);
void nuvio_search_history_view_destroy(RustSearchHistoryView *view);
uint64_t nuvio_search_history_view_revision(const RustSearchHistoryView *view);
size_t nuvio_search_history_view_count(const RustSearchHistoryView *view);
const char *nuvio_search_history_view_query(const RustSearchHistoryView *view, size_t index);
int nuvio_search_history_record(const RustSearchHistory *history, const char *query);
void nuvio_search_history_remove(const RustSearchHistory *history, size_t index);
void nuvio_search_history_clear(const RustSearchHistory *history);
void nuvio_search_history_logout(const RustSearchHistory *history);
#endif
