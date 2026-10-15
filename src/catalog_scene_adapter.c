/* Read-only live display views. Rust owns the model, validation and JSON. */
#include "catalog_scene_adapter.h"
#include <limits.h>
#include <string.h>

int catalog_scene_capture_title(RustSceneCapture *capture, const CatItem *item) {
  RustCaptureTitle view = {0};
  RustCaptureCast cast[CAT_ELENCO_MAX];
  unsigned field;
  int index;
  if (!item || item->nElenco < 0 || item->nElenco > CAT_ELENCO_MAX ||
      item->nTemporadas < 0 || item->nTemporadas > CAT_TEMP_MAX)
    return nuvio_scene_capture_title(capture, NULL);
  view.source = item->rust_title;
  for (field = 0; field < TITLE_FIELD_COUNT; field++) view.text[field] = catalog_title_text(item, field);
  view.rating = item->nota; view.tmdb_id = item->tmdb > 0 ? (uint64_t)item->tmdb : 0;
  view.percentage = item->progresso; view.season = item->temporada; view.episode = item->episodio;
  view.remaining_minutes = item->restanteMin; view.last_watched_ms = item->retomadoMs;
  view.on_watchlist = item->naLista; view.in_collection = item->naColecao;
  view.cast = cast; view.cast_count = (size_t)item->nElenco; view.cast_capacity = CAT_ELENCO_MAX;
  view.seasons = item->temporadas; view.season_count = (size_t)item->nTemporadas; view.season_capacity = CAT_TEMP_MAX;
  for (index = 0; index < item->nElenco; index++) {
#define FIELD(target, member) cast[index].target = (RustCaptureText){item->elenco[index].member, sizeof item->elenco[index].member}
    FIELD(name, nome); FIELD(role, papel); FIELD(photo, foto);
#undef FIELD
    cast[index].tmdb_id = item->elenco[index].tmdb > 0 ? (uint64_t)item->elenco[index].tmdb : 0;
  }
  return nuvio_scene_capture_title(capture, &view);
}

static const char *row_text(const RustRowSnapshot *source, unsigned field, const char *overlay, size_t capacity) {
  if (!source) return overlay;
  return catalog_text_with_overlay(nuvio_row_text(source, field), overlay, capacity);
}
int catalog_scene_capture_row(RustSceneCapture *capture, const CatFileira *row, const RustRowSnapshot *source) {
  RustCaptureRow view = {0};
  const RustRowInfo *info;
  if (!row || row->ini < 0 || row->n < 0 || (row->estado != 0 && row->estado != 1))
    return nuvio_scene_capture_row(capture, NULL);
  view.source = source;
#define FIELD(target, selector, member) view.target = row_text(source, selector, row->member, sizeof row->member)
  FIELD(key, ROW_KEY, chave); FIELD(name, ROW_NAME, titulo); FIELD(media_type, ROW_MEDIA_TYPE, tipo);
  FIELD(addon_url, ROW_ADDON_URL, base); FIELD(catalog_id, ROW_CATALOG_ID, catId);
#undef FIELD
  view.start = (size_t)row->ini; view.count = (size_t)row->n;
  view.state = row->estado ? ROW_CONFIRMED_EMPTY : row->n ? ROW_READY : ROW_LOADING;
  info = nuvio_row_info(source);
  if (info && row->estado == (info->state == ROW_CONFIRMED_EMPTY) &&
      info->end - info->start == view.count) view.state = info->state;
  return nuvio_scene_capture_row(capture, &view);
}
const RustSceneSnapshot *catalog_scene_capture(const CatItem *items, int count, const CatFileira *rows, int row_count) {
  RustSceneCapture *capture;
  int index;
  if (count < 0 || row_count < 0 || (count && !items) || (row_count && !rows)) return NULL;
  capture = nuvio_scene_capture_create();
  for (index = 0; index < count; index++)
    if (!catalog_scene_capture_title(capture, &items[index])) goto failed;
  for (index = 0; index < row_count; index++)
    if (!catalog_scene_capture_row(capture, &rows[index], NULL)) goto failed;
  return nuvio_scene_capture_finish(capture);
failed:
  nuvio_scene_capture_destroy(capture);
  return NULL;
}
