/* Native actions and snapshot lifetime only. Rust owns rules, scope and storage. */
#include "search_history.h"
#include <stdio.h>
#include <pthread.h>
static RustSearchHistory *history;
static pthread_mutex_t lifetime = PTHREAD_MUTEX_INITIALIZER;
static RustSearchHistoryView *view;
static void refresh(void) {
  search_history_step();
  if (view && nuvio_search_history_view_revision(view) == nuvio_search_history_revision(history)) return;
  nuvio_search_history_view_destroy(view);
  view = nuvio_search_history_snapshot(history);
}
void search_history_init(RustAccount *account, const char *directory) {
  if (!history) history = nuvio_search_history_create(account, directory);
  printf("[search] Rust recent-search worker %s\n", history ? "initialized" : "unavailable");
}
void search_history_shutdown(void) {
  nuvio_search_history_view_destroy(view); view = NULL;
  pthread_mutex_lock(&lifetime);
  RustSearchHistory *stopped = history; history = NULL;
  pthread_mutex_unlock(&lifetime);
  nuvio_search_history_destroy(stopped);
}
void search_history_step(void) { nuvio_search_history_step(history); }
uint64_t search_history_revision(void) { search_history_step(); return nuvio_search_history_revision(history); }
uint64_t search_history_snapshot_revision(void) { return nuvio_search_history_view_revision(view); }
int search_history_count(void) { refresh(); return (int)nuvio_search_history_view_count(view); }
const char *search_history_query(int index) {
  /* count/record/remove/clear refresh at action and draw boundaries. */
  return index >= 0 ? nuvio_search_history_view_query(view, (size_t)index) : "";
}
int search_history_record(const char *query) { int accepted = nuvio_search_history_record(history, query); refresh(); return accepted; }
void search_history_remove(int index) { if (index >= 0) nuvio_search_history_remove(history, (size_t)index); refresh(); }
void search_history_clear(void) { nuvio_search_history_clear(history); refresh(); }
/* Account logout can be invoked on the sync worker. Do not release UI views here. */
void search_history_logout(void) {
  pthread_mutex_lock(&lifetime);
  nuvio_search_history_logout(history);
  pthread_mutex_unlock(&lifetime);
}
#ifdef NV_NATIVE_UI_TEST
void search_history_test_attach(RustSearchHistory *test_history) {
  nuvio_search_history_view_destroy(view); view = NULL;
  history = test_history;
}
#endif
