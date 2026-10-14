/* Real catalog/model operations for Rust ownership regression tests. */
#include "catalog_item.h"
#include <stdlib.h>
#include <stdio.h>

CatItem *native_title_create(const RustTitleSnapshot *snapshot) {
  CatItem *item = calloc(1, sizeof *item);
  if (!item) { nuvio_title_release(snapshot); return NULL; }
  if (item && !catalog_item_from_rust(item, snapshot)) { free(item); item = NULL; }
  return item;
}
CatItem *native_title_clone(const CatItem *source) {
  CatItem *item = calloc(1, sizeof *item);
  if (item) catalog_item_copy(item, source);
  return item;
}
void native_title_destroy(CatItem *item) { catalog_item_clear(item); free(item); }
const char *native_title_text(const CatItem *item, unsigned field) { return catalog_title_text(item, field); }
void native_title_overlay_poster(CatItem *item, const char *poster) {
  snprintf(item->poster, sizeof item->poster, "%s", poster);
}
void native_title_publish(const CatItem *item) { cat_definir_tudo(item, item ? 1 : 0, NULL, 0); }
void native_title_update(const CatItem *item) { cat_atualizar_item(0, item); }
const CatItem *native_title_current(void) { return cat_n() ? cat_item(0) : NULL; }
void native_title_frame(void) { cat_quadro(); }
int native_title_find(const char *id) { return cat_indice_por_imdb(id); }
