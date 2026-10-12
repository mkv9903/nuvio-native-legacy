/* Historical C data-access names. The registry and all returned strings are
 * Rust-owned; pointers remain valid for the caller's pinned reader. */
#include "addon_registry.h"
#include "addons.h"
#include <string.h>

/* Historical names remain for C capabilities awaiting their own rewrite. */
int addons_n(void) { return addon_registry_count(); }
unsigned addons_versao(void) { return addon_registry_version(); }
int addons_perfil_da_lista(void) { const RustAddonSnapshotView *view = addon_registry_view(); return view->authoritative ? (int)view->profile : 0; }
static const RustAddonView *item(int index) {
  const RustAddonSnapshotView *view = addon_registry_view();
  return index >= 0 && (size_t)index < view->count ? &view->addons[index] : NULL;
}
const char *addons_nome(int index) { const RustAddonView *value = item(index); return value ? value->name : ""; }
const char *addons_base(int index) { const RustAddonView *value = item(index); return value ? value->base : ""; }
const char *addons_id_manifesto(int index) { const RustAddonView *value = item(index); return value ? value->id : ""; }
int addons_ativo(int index) { const RustAddonView *value = item(index); return value ? value->enabled : 0; }
int addons_sondado(int index) { const RustAddonView *value = item(index); return value ? value->known : 0; }
int addons_tem_catalogo(int index) { const RustAddonView *value = item(index); return value && value->enabled && value->catalog; }
int addons_fornece(int index, int resource) {
  const RustAddonView *value = item(index);
  if (!value) return 0;
  if (resource == ADD_CATALOGO) return value->catalog;
  if (resource == ADD_STREAM) return value->stream;
  if (resource == ADD_META) return value->metadata;
  return value->subtitles;
}
const char *addons_nome_por_id(const char *id) {
  const RustAddonSnapshotView *view = addon_registry_view(); size_t i;
  if (id && *id) for (i = 0; i < view->count; i++) if (!strcmp(view->addons[i].id, id)) return view->addons[i].name;
  return "";
}
const char *addons_base_por_id(const char *id) {
  const RustAddonSnapshotView *view = addon_registry_view(); size_t i;
  if (id && *id) for (i = 0; i < view->count; i++) if (!strcmp(view->addons[i].id, id)) return view->addons[i].base;
  return "";
}
int addons_aceita_id(int index, const char *type, const char *id) {
  RustAddonSnapshot *snapshot = addon_registry_capture();
  int result = index >= 0 ? nuvio_addons_snapshot_accepts(snapshot, (size_t)index, 3, type ? type : "", id) : 0;
  nuvio_addons_snapshot_destroy(snapshot);
  return result;
}
int addons_catalogos_canal(int index, AddCatCanal *output, int capacity) {
  RustAddonSnapshot *snapshot = addon_registry_capture();
  const RustAddonView *value = item(index); size_t i; int count = 0;
  int loaded = value && nuvio_addons_snapshot_has_manifest(snapshot, (size_t)index);
  nuvio_addons_snapshot_destroy(snapshot);
  if (!loaded) return -1;
  for (i = 0; output && i < value->catalog_count && count < capacity; i++) {
    const RustAddonCatalogView *catalog = &value->catalogs[i];
    if (catalog->channel) output[count++] = (AddCatCanal){ catalog->media_type, catalog->id, catalog->name };
  }
  return count;
}
