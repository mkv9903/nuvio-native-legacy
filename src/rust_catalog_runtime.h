#ifndef NV_RUST_CATALOG_RUNTIME_H
#define NV_RUST_CATALOG_RUNTIME_H
#include "rust_addons.h"
#include "rust_scene.h"
typedef struct RustCatalogRuntime RustCatalogRuntime;
typedef struct RustCatalogWork RustCatalogWork;
typedef struct RustCatalogSceneView RustCatalogSceneView;
typedef struct RustContentLease RustContentLease;
RustContentLease *nuvio_catalog_content_begin(const RustCatalogRuntime *runtime);
int nuvio_catalog_content_current(const RustContentLease *lease);
void nuvio_catalog_content_end(RustContentLease *lease);
const RustCatalogRuntime *nuvio_catalog_create(const RustAccount *account, const RustAddons *addons, const char *directory);
void nuvio_catalog_destroy(const RustCatalogRuntime *runtime);
void nuvio_catalog_step(const RustCatalogRuntime *runtime);
void nuvio_catalog_logout(const RustCatalogRuntime *runtime);
const RustCatalogWork *nuvio_catalog_begin(const RustCatalogRuntime *runtime);
const RustCatalogWork *nuvio_catalog_work_retain(const RustCatalogWork *work);
void nuvio_catalog_work_release(const RustCatalogWork *work);
int nuvio_catalog_work_current(const RustCatalogWork *work);
const RustSceneSnapshot *nuvio_catalog_load(const RustCatalogWork *work, const char *base, const char *type, const char *id, const char *origin, size_t limit, int *responded);
const RustSceneSnapshot *nuvio_catalog_load_index(const RustCatalogWork *work, size_t addon, size_t catalog, size_t limit, int *responded);
int nuvio_catalog_accept(const RustCatalogWork *work, const RustSceneSnapshot *scene, int save);
int nuvio_catalog_row_reusable(const RustCatalogWork *work, const char *base, const char *type, const char *id);
uint64_t nuvio_catalog_revision(const RustCatalogRuntime *runtime);
RustCatalogSceneView *nuvio_catalog_view(const RustCatalogRuntime *runtime);
void nuvio_catalog_view_release(RustCatalogSceneView *view);
RustCatalogSceneView *nuvio_catalog_view_clone(const RustCatalogSceneView *view);
int nuvio_catalog_view_context_current(const RustCatalogSceneView *view);
RustAddonSnapshot *nuvio_catalog_view_addons(const RustCatalogSceneView *view);
int nuvio_catalog_view_current(const RustCatalogSceneView *view);
uint64_t nuvio_catalog_view_activation(const RustCatalogSceneView *view);
int nuvio_catalog_view_cached(const RustCatalogSceneView *view);
int nuvio_catalog_view_requires_apply(const RustCatalogSceneView *view);
const RustSceneSnapshot *nuvio_catalog_view_scene(const RustCatalogSceneView *view);
#endif
