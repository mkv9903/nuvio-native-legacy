#ifndef NV_RUST_SCENE_H
#define NV_RUST_SCENE_H
#include "rust_title.h"
typedef struct RustSceneSnapshot RustSceneSnapshot;
typedef struct RustRowSnapshot RustRowSnapshot;
enum RustRowField {
  ROW_KEY, ROW_NAME, ROW_MEDIA_TYPE, ROW_ADDON_URL, ROW_CATALOG_ID, ROW_REQUEST_URL
};
enum RustRowState { ROW_LOADING, ROW_READY, ROW_CONFIRMED_EMPTY };
typedef struct { size_t start, end; uint32_t state; } RustRowInfo;
const RustSceneSnapshot *nuvio_scene_retain(const RustSceneSnapshot *scene);
void nuvio_scene_release(const RustSceneSnapshot *scene);
size_t nuvio_scene_title_count(const RustSceneSnapshot *scene);
size_t nuvio_scene_row_count(const RustSceneSnapshot *scene);
/* Indexed title/row access creates an owned reference; release each separately.
 * Row text/info is borrowed from a retained row, independent of scene lifetime. */
const RustTitleSnapshot *nuvio_scene_title(const RustSceneSnapshot *scene, size_t index);
const RustRowSnapshot *nuvio_scene_row(const RustSceneSnapshot *scene, size_t index);
const RustRowSnapshot *nuvio_row_retain(const RustRowSnapshot *row);
void nuvio_row_release(const RustRowSnapshot *row);
const char *nuvio_row_text(const RustRowSnapshot *row, unsigned field);
void nuvio_row_copy_text(const RustRowSnapshot *row, unsigned field, char *output, size_t capacity);
const RustRowInfo *nuvio_row_info(const RustRowSnapshot *row);
#endif
