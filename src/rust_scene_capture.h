#ifndef NV_RUST_SCENE_CAPTURE_H
#define NV_RUST_SCENE_CAPTURE_H
#include "rust_scene.h"
typedef struct RustSceneCapture RustSceneCapture;
typedef struct { const char *value; size_t capacity; } RustCaptureText;
typedef struct {
  RustCaptureText name, role, photo;
  uint64_t tmdb_id;
} RustCaptureCast;
typedef struct {
  const RustTitleSnapshot *source;
  const char *text[TITLE_FIELD_COUNT];
  int32_t rating;
  uint64_t tmdb_id;
  int32_t percentage, season, episode, remaining_minutes;
  int64_t last_watched_ms;
  int32_t on_watchlist, in_collection;
  const RustCaptureCast *cast;
  size_t cast_count, cast_capacity;
  const int32_t *seasons;
  size_t season_count, season_capacity;
} RustCaptureTitle;
typedef struct {
  const RustRowSnapshot *source;
  const char *key, *name, *media_type, *addon_url, *catalog_id;
  size_t start, count;
  uint32_t state;
} RustCaptureRow;
/* All inputs are borrowed for the call only. Rust copies live values into its
 * own scene model; it never serializes these views or imports a C cache file.
 * One failed input rejects the whole capture, including all earlier records. */
RustSceneCapture *nuvio_scene_capture_create(void);
int nuvio_scene_capture_title(RustSceneCapture *capture, const RustCaptureTitle *title);
int nuvio_scene_capture_row(RustSceneCapture *capture, const RustCaptureRow *row);
/* Finish consumes the capture and returns an owned scene, or NULL on failure. */
const RustSceneSnapshot *nuvio_scene_capture_finish(RustSceneCapture *capture);
void nuvio_scene_capture_destroy(RustSceneCapture *capture);
#endif
