#ifndef NV_CATALOG_SCENE_ADAPTER_H
#define NV_CATALOG_SCENE_ADAPTER_H
#include "catalog_item.h"
#include "rust_scene_capture.h"
/* Callers keep coherent, owned catalog inputs alive until capture returns. */
int catalog_scene_capture_title(RustSceneCapture *capture, const CatItem *item);
int catalog_scene_capture_row(RustSceneCapture *capture, const CatFileira *row, const RustRowSnapshot *source);
const RustSceneSnapshot *catalog_scene_capture(const CatItem *items, int count, const CatFileira *rows, int row_count);
#endif
