#ifndef NV_CATALOG_RUNTIME_H
#define NV_CATALOG_RUNTIME_H
#include "rust_catalog_runtime.h"
#include "catalogo.h"
void catalog_runtime_init(RustAccount *account, const RustAddons *addons, const char *directory);
void catalog_runtime_step(void);
void catalog_runtime_logout(void);
int catalog_runtime_enabled(void);
void catalog_runtime_cycle_begin(void);
void catalog_runtime_cycle_end(void);
const RustCatalogWork *catalog_runtime_work(void); /* borrowed until cycle end */
void catalog_runtime_use(const RustCatalogWork *work); /* retains a worker's job */
int catalog_runtime_current(void);
void catalog_runtime_mark_published(void);
int catalog_runtime_published_current(void);
int catalog_runtime_display_current(void);
/* Capture the displayed profile along with an owned title for metadata work. */
int catalog_metadata_copy(int index, CatItem *title);
int catalog_metadata_current(void);
void catalog_metadata_end(void);
int catalog_content_begin(void);
void catalog_content_end(void);
int catalog_runtime_capture(int save);
int catalog_runtime_row_reusable(const CatFileira *row);
int catalog_cached_row_allowed(const char *key);
/* Copies the retained scene into the existing catalog publication path. */
int catalog_apply_scene(const RustSceneSnapshot *scene, int cached);
int catalog_apply_scene_view(const RustSceneSnapshot *scene, int cached, const RustCatalogSceneView *view, unsigned expected_revision);
int catalog_publish_job(const CatItem *titles, int count, const CatFileira *rows, int row_count);
const RustSceneSnapshot *catalog_capture_scene(void);
void catalog_mark_cached(int cached);
#endif
