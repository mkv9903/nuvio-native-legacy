/* Layout measurements only; assertions live in the Rust native driver. */
#include "rust_addons.h"
#include <stddef.h>

size_t rust_addons_test_view_size(int kind) {
  switch (kind) {
    case 0: return sizeof(RustAddonSnapshotView);
    case 1: return sizeof(RustAddonView);
    case 2: return sizeof(RustAddonCatalogView);
    default: return 0;
  }
}
size_t rust_addons_test_view_offset(int kind, size_t field) {
#define FIELD(type, name) offsetof(type, name)
  static const size_t snapshot[] = {
    FIELD(RustAddonSnapshotView, revision), FIELD(RustAddonSnapshotView, list_revision),
    FIELD(RustAddonSnapshotView, addons), FIELD(RustAddonSnapshotView, count),
    FIELD(RustAddonSnapshotView, profile), FIELD(RustAddonSnapshotView, addon_profile),
    FIELD(RustAddonSnapshotView, authoritative), FIELD(RustAddonSnapshotView, pending),
    FIELD(RustAddonSnapshotView, status), FIELD(RustAddonSnapshotView, error)
  };
  static const size_t addon[] = {
    FIELD(RustAddonView, name), FIELD(RustAddonView, base), FIELD(RustAddonView, id),
    FIELD(RustAddonView, enabled), FIELD(RustAddonView, known), FIELD(RustAddonView, catalog),
    FIELD(RustAddonView, stream), FIELD(RustAddonView, subtitles), FIELD(RustAddonView, metadata),
    FIELD(RustAddonView, silent_queries), FIELD(RustAddonView, catalogs), FIELD(RustAddonView, catalog_count)
  };
  static const size_t catalog[] = {
    FIELD(RustAddonCatalogView, id), FIELD(RustAddonCatalogView, media_type), FIELD(RustAddonCatalogView, name),
    FIELD(RustAddonCatalogView, searchable), FIELD(RustAddonCatalogView, requires_search), FIELD(RustAddonCatalogView, channel)
  };
#undef FIELD
  if (kind == 0 && field < sizeof snapshot / sizeof *snapshot) return snapshot[field];
  if (kind == 1 && field < sizeof addon / sizeof *addon) return addon[field];
  if (kind == 2 && field < sizeof catalog / sizeof *catalog) return catalog[field];
  return (size_t)-1;
}
