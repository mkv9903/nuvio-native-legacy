#ifndef NV_ADDON_REGISTRY_H
#define NV_ADDON_REGISTRY_H
#include "rust_addons.h"

/* UI readers refresh once at a frame boundary. A worker pins its parent's
 * snapshot for the entire job; pointers stay valid until that reader refreshes. */
void addon_registry_init(RustAccount *account, const char *directory);
void addon_registry_step(int force_primary, int allow_periodic);
void addon_registry_refresh(void);
void addon_registry_use(const RustAddonSnapshot *snapshot);
RustAddonSnapshot *addon_registry_capture(void);
const RustAddonSnapshotView *addon_registry_view(void);
const RustAddonView *addon_registry_items(void);
int addon_registry_count(void);
unsigned addon_registry_version(void);
int addon_registry_reader_current(void);
int addon_registry_snapshot_current(const RustAddonSnapshot *snapshot);
void addon_registry_request_refresh(void);
void addon_registry_logout(void);
int addon_registry_toggle(int index); /* -1 rejected; otherwise new 0/1 state */
int addon_registry_install(const char *name, const char *url);
int addon_registry_attach_manifest(int index, const char *body);
int64_t addon_registry_report_response(int index, int responded);
char *addon_registry_manifest_url(int index);
char *addon_registry_resource_url(int index, const char *resource, const char *type, const char *id, const char *encoded_extras);

#ifdef NV_NATIVE_UI_TEST
void addon_registry_test_attach(RustAddons *controller);
#endif
#endif
