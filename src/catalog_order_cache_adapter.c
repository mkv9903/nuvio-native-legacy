/* C sync bridge to fresh versioned Rust Home-order storage. */
#include "catordemcache.h"
#include "data.h"
#include "rust_catalog_order.h"
#include <stdio.h>

int catordem_cache_carregar(int profile, const char *account) {
  int changed = nuvio_home_order_cache_load(dados_dir(), account, profile > 0 ? (unsigned)profile : 0);
  if (changed) printf("[catalogs] Restored Home order for profile %d\n", profile);
  return changed;
}
int catordem_cache_gravar(int profile, const char *account, const char *body) {
  int saved = nuvio_home_order_cache_save(dados_dir(), account, profile > 0 ? (unsigned)profile : 0, body);
  if (saved) printf("[catalogs] Saved Home order for profile %d\n", profile);
  return saved;
}
void catordem_cache_esquecer(void) { nuvio_home_order_cache_clear(dados_dir()); }
