/* Renderer compatibility only. Parsing, state, ordering and storage live in Rust. */
#include "catordem.h"
#include "rust_catalog_order.h"
#include <limits.h>
#include <pthread.h>

static pthread_key_t text_key;
static pthread_once_t text_once = PTHREAD_ONCE_INIT;
static void release_text(void *snapshot) { nuvio_home_order_snapshot_destroy(snapshot); }
static void create_text_key(void) { pthread_key_create(&text_key, release_text); }

int catordem_ler(const char *body) { return nuvio_home_order_receive(body); }
unsigned catordem_revisao(void) { return (unsigned)nuvio_home_order_revision(); }
int catordem_tem_ordem(void) { return nuvio_home_order_configured(); }
int catordem_n(void) { size_t count = nuvio_home_order_count(); return count > INT_MAX ? INT_MAX : (int)count; }
const char *catordem_chave(int index) {
  RustHomeOrderSnapshot *snapshot;
  pthread_once(&text_once, create_text_key);
  snapshot = pthread_getspecific(text_key);
  if (!snapshot || nuvio_home_order_snapshot_revision(snapshot) != nuvio_home_order_revision()) {
    nuvio_home_order_snapshot_destroy(snapshot);
    snapshot = nuvio_home_order_snapshot();
    pthread_setspecific(text_key, snapshot);
  }
  return nuvio_home_order_snapshot_key(snapshot, index < 0 ? (size_t)-1 : (size_t)index);
}
int catordem_oculta(const char *key, const char *provider_key) { return nuvio_home_order_hidden(key, provider_key); }
int catordem_unir(const char *const *local, int count, int *output, int capacity) {
  if (count <= 0 || capacity <= 0) return 0;
  return (int)nuvio_home_order_union(local, (size_t)count, output, (size_t)capacity);
}
int catordem_tem_ocultar_nao_lancados(void) { return nuvio_home_order_preference(0) >= 0; }
int catordem_ocultar_nao_lancados(void) { return nuvio_home_order_preference(0) == 1; }
int catordem_tem_ocultar_sublinhado(void) { return nuvio_home_order_preference(1) >= 0; }
int catordem_ocultar_sublinhado(void) { return nuvio_home_order_preference(1) == 1; }
void catordem_esquecer(void) { nuvio_home_order_reset(); }
