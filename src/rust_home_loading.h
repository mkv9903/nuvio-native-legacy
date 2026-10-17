#ifndef NV_RUST_HOME_LOADING_H
#define NV_RUST_HOME_LOADING_H
#include "rust_catalog_runtime.h"
typedef struct RustHomePlan RustHomePlan;
typedef struct {
  size_t requested, responded, empty, failed, rows, remaining;
} RustHomeLoadStats;
typedef int (*RustHomePublish)(const RustSceneSnapshot *, int, const RustHomeLoadStats *, void *);
typedef void (*RustManifestProgress)(size_t,size_t,void *);
RustHomePlan *nuvio_home_prepare(const RustCatalogWork *work, RustManifestProgress progress, void *context);
void nuvio_home_destroy(RustHomePlan *plan);
uint32_t nuvio_home_profile(const RustHomePlan *plan);
size_t nuvio_home_count(const RustHomePlan *plan);
RustAddonSnapshot *nuvio_home_addons(const RustHomePlan *plan);
/* Owned text; release with nuvio_text_destroy. No fixed request buffers. */
char *nuvio_home_text(const RustHomePlan *plan, size_t index, uint32_t field);
uint32_t nuvio_home_flags(const RustHomePlan *plan, size_t index);
void nuvio_home_choice(RustHomePlan *plan, size_t index, int64_t position, uint32_t flags);
int nuvio_home_load(RustHomePlan *plan, const RustCatalogWork *work, size_t rows, size_t items,
                    RustHomePublish publish, void *context, RustHomeLoadStats *stats);
#endif
