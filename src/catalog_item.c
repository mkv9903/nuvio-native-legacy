/* Ownership bridge only; application title values remain Rust-owned. */
#include "catalog_item.h"
#include <limits.h>
#include <string.h>

void catalog_item_copy(CatItem *destination, const CatItem *source) {
  if (!destination || !source || destination == source) return;
  memcpy(destination, source, sizeof *destination);
  destination->rust_title = nuvio_title_retain(source->rust_title);
}
void catalog_item_clear(CatItem *item) {
  if (!item) return;
  nuvio_title_release(item->rust_title);
  memset(item, 0, sizeof *item);
}
void catalog_item_replace(CatItem *destination, const CatItem *source) {
  CatItem copy;
  if (!destination || !source || destination == source) return;
  catalog_item_copy(&copy, source);
  catalog_item_clear(destination);
  memcpy(destination, &copy, sizeof copy); /* transfer the new owned reference */
}
void catalog_item_move(CatItem *destination, CatItem *source) {
  if (!destination || !source || destination == source) return;
  catalog_item_clear(destination);
  memcpy(destination, source, sizeof *destination);
  memset(source, 0, sizeof *source);
}
void catalog_items_copy(CatItem *destination, const CatItem *source, size_t count) {
  size_t index;
  for (index = 0; index < count; index++) catalog_item_copy(destination + index, source + index);
}
void catalog_items_clear(CatItem *items, size_t count) {
  size_t index;
  for (index = 0; items && index < count; index++) catalog_item_clear(items + index);
}
int catalog_item_from_rust(CatItem *destination, const RustTitleSnapshot *title) {
  uint64_t tmdb;
  if (!destination || !title) return 0;
  memset(destination, 0, sizeof *destination);
  destination->rust_title = title;
#define PROJECT(member, field) nuvio_title_copy_text(title, field, destination->member, sizeof destination->member)
  PROJECT(imdb, TITLE_ID); PROJECT(tipo, TITLE_MEDIA_TYPE); PROJECT(titulo, TITLE_NAME);
  PROJECT(poster, TITLE_POSTER); PROJECT(backdrop, TITLE_BACKGROUND); PROJECT(backdropCatalogo, TITLE_BACKGROUND);
  PROJECT(logo, TITLE_LOGO); PROJECT(genero, TITLE_GENRE_TEXT); PROJECT(meta, TITLE_METADATA_TEXT);
  PROJECT(sinopse, TITLE_DESCRIPTION); PROJECT(pais, TITLE_COUNTRY); PROJECT(origem, TITLE_ORIGIN);
#undef PROJECT
  destination->nota = nuvio_title_rating(title);
  tmdb = nuvio_title_tmdb_id(title);
  destination->tmdb = tmdb <= LONG_MAX ? (long)tmdb : 0;
  return 1;
}
const char *catalog_title_text(const CatItem *item, unsigned field) {
  const char *overlay = "", *complete;
  size_t capacity = 1, length;
  if (!item) return "";
#define OVERLAY(selector, member) case selector: overlay = item->member; capacity = sizeof item->member; break
  switch (field) {
    OVERLAY(TITLE_ID, imdb); OVERLAY(TITLE_MEDIA_TYPE, tipo); OVERLAY(TITLE_NAME, titulo);
    OVERLAY(TITLE_POSTER, poster); OVERLAY(TITLE_BACKGROUND, backdrop); OVERLAY(TITLE_LOGO, logo);
    OVERLAY(TITLE_GENRE_TEXT, genero); OVERLAY(TITLE_METADATA_TEXT, meta);
    OVERLAY(TITLE_DESCRIPTION, sinopse); OVERLAY(TITLE_COUNTRY, pais); OVERLAY(TITLE_ORIGIN, origem);
    case TITLE_IMDB_ID: return catalog_title_imdb_id(item);
    default: return "";
  }
#undef OVERLAY
  if (!item->rust_title) return overlay;
  complete = nuvio_title_text(item->rust_title, field);
  if (field == TITLE_ID) return complete; /* Provider identity is immutable. */
  length = strlen(complete);
  if (length >= capacity) length = capacity - 1;
  while (length && (((unsigned char)complete[length] & 0xc0) == 0x80)) length--;
  /* Unmigrated enrichment code can still change its own overlay. */
  if (!memcmp(overlay, complete, length) && overlay[length] == 0) return complete;
  return overlay;
}
const char *catalog_title_id(const CatItem *item) { return catalog_title_text(item, TITLE_ID); }
const char *catalog_title_imdb_id(const CatItem *item) {
  const char *id, *hint;
  if (!item) return "";
  if (!item->rust_title) return item->imdb;
  hint = nuvio_title_text(item->rust_title, TITLE_IMDB_ID);
  if (*hint) return hint;
  id = nuvio_title_text(item->rust_title, TITLE_ID);
  return id[0] == 't' && id[1] == 't' ? id : "";
}
