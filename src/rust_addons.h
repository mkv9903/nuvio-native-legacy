#ifndef NV_RUST_ADDONS_H
#define NV_RUST_ADDONS_H
#include "rust_account.h"

typedef struct RustAddons RustAddons;
typedef struct RustAddonSnapshot RustAddonSnapshot;
typedef struct {
  const char *id, *media_type, *name;
  int searchable, requires_search, channel;
} RustAddonCatalogView;
typedef struct {
  const char *name, *base, *id;
  int enabled, known, catalog, stream, subtitles, metadata;
  uint32_t silent_queries;
  const RustAddonCatalogView *catalogs;
  size_t catalog_count;
} RustAddonView;
enum {
  RUST_ADDONS_UNAVAILABLE, RUST_ADDONS_LOADING, RUST_ADDONS_READY,
  RUST_ADDONS_RETRY_PENDING, RUST_ADDONS_UNSAVED, RUST_ADDONS_RECOVERY_PAUSED
};
typedef struct {
  uint64_t revision, list_revision;
  const RustAddonView *addons;
  size_t count;
  uint32_t profile, addon_profile;
  int authoritative, pending, status;
  const char *error;
} RustAddonSnapshotView;

/* Each reader retains its snapshot until its final pointer access. Clones
 * share immutable data. Actions copy inputs, validate the originating profile
 * and list revision, and queue disk/network work. No strings are truncated. */
RustAddons *nuvio_addons_create(const RustAccount *account, const char *directory);
void nuvio_addons_destroy(RustAddons *addons);
void nuvio_addons_step(const RustAddons *addons, int force_primary);
void nuvio_addons_refresh(const RustAddons *addons);
/* Call after account logout; cleanup runs on the addon worker. */
void nuvio_addons_logout(const RustAddons *addons);
uint64_t nuvio_addons_revision(const RustAddons *addons);
RustAddonSnapshot *nuvio_addons_snapshot(const RustAddons *addons);
RustAddonSnapshot *nuvio_addons_snapshot_clone(const RustAddonSnapshot *snapshot);
const RustAddonSnapshotView *nuvio_addons_snapshot_view(const RustAddonSnapshot *snapshot);
void nuvio_addons_snapshot_destroy(RustAddonSnapshot *snapshot);
int nuvio_addons_set_enabled(const RustAddons *addons, const RustAddonSnapshot *snapshot, size_t index, int enabled);
int nuvio_addons_install(const RustAddons *addons, const RustAddonSnapshot *snapshot, const char *name, const char *url);
/* Returns -1 while capabilities are unknown, otherwise 0/1. Resource kinds
 * are catalog=0, stream=1, subtitles=2, metadata=3. */
int nuvio_addons_snapshot_accepts(const RustAddonSnapshot *snapshot, size_t index, int kind, const char *media_type, const char *id);
/* Returned request URLs are independently owned; use nuvio_text_destroy. */
char *nuvio_addons_snapshot_url(const RustAddonSnapshot *snapshot, size_t index, const char *resource, const char *media_type, const char *id);
#endif
