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
  const RustTitleInfo *info;
  size_t index;
  if (!destination || !title) return 0;
  memset(destination, 0, sizeof *destination);
  destination->rust_title = title;
#define PROJECT(member, field) nuvio_title_copy_text(title, field, destination->member, sizeof destination->member)
  PROJECT(imdb, TITLE_ID); PROJECT(tipo, TITLE_MEDIA_TYPE); PROJECT(titulo, TITLE_NAME);
  PROJECT(poster, TITLE_POSTER); PROJECT(backdrop, TITLE_BACKGROUND); PROJECT(backdropCatalogo, TITLE_CATALOG_BACKGROUND);
  PROJECT(logo, TITLE_LOGO); PROJECT(genero, TITLE_GENRE_TEXT); PROJECT(meta, TITLE_METADATA_TEXT);
  PROJECT(sinopse, TITLE_DESCRIPTION); PROJECT(pais, TITLE_COUNTRY); PROJECT(origem, TITLE_ORIGIN);
  PROJECT(backdropTmdb, TITLE_TMDB_BACKGROUND); PROJECT(backdropTrakt, TITLE_TRAKT_BACKGROUND);
  PROJECT(logoIdioma, TITLE_LOGO_LANGUAGE); PROJECT(logoIdiomaUrl, TITLE_LOGO_LANGUAGE_URL);
  PROJECT(classificacao, TITLE_CLASSIFICATION); PROJECT(direcao, TITLE_DIRECTOR);
  PROJECT(provNome, TITLE_SUBSCRIPTION_NAME); PROJECT(provLogo, TITLE_SUBSCRIPTION_LOGO);
  PROJECT(alugNome, TITLE_RENTAL_NAME); PROJECT(alugLogo, TITLE_RENTAL_LOGO);
  PROJECT(compNome, TITLE_PURCHASE_NAME); PROJECT(compLogo, TITLE_PURCHASE_LOGO);
  PROJECT(nomeEpisodio, TITLE_EPISODE_NAME);
#undef PROJECT
  destination->nota = nuvio_title_rating(title);
  tmdb = nuvio_title_tmdb_id(title);
  destination->tmdb = tmdb <= LONG_MAX ? (long)tmdb : 0;
  info = nuvio_title_info(title);
  destination->progresso = (int)info->percentage;
  destination->temporada = info->season <= INT_MAX ? (int)info->season : INT_MAX;
  destination->episodio = info->episode <= INT_MAX ? (int)info->episode : INT_MAX;
  destination->restanteMin = info->remaining_minutes <= INT_MAX ? (int)info->remaining_minutes : INT_MAX;
  destination->retomadoMs = info->last_watched_ms;
  destination->naLista = info->on_watchlist; destination->naColecao = info->in_collection;
  destination->nElenco = info->cast_count < CAT_ELENCO_MAX ? (int)info->cast_count : CAT_ELENCO_MAX;
  destination->nTemporadas = info->season_count < CAT_TEMP_MAX ? (int)info->season_count : CAT_TEMP_MAX;
  for (index = 0; index < (size_t)destination->nElenco; index++) {
    nuvio_title_copy_cast_text(title, index, 0, destination->elenco[index].nome, sizeof destination->elenco[index].nome);
    nuvio_title_copy_cast_text(title, index, 1, destination->elenco[index].papel, sizeof destination->elenco[index].papel);
    nuvio_title_copy_cast_text(title, index, 2, destination->elenco[index].foto, sizeof destination->elenco[index].foto);
    tmdb = nuvio_title_cast_tmdb_id(title, index);
    destination->elenco[index].tmdb = tmdb <= LONG_MAX ? (long)tmdb : 0;
  }
  for (index = 0; index < (size_t)destination->nTemporadas; index++) {
    uint32_t season = nuvio_title_season(title, index);
    destination->temporadas[index] = season <= INT_MAX ? (int)season : INT_MAX;
  }
  return 1;
}
const char *catalog_title_text(const CatItem *item, unsigned field) {
  const char *overlay = "", *complete;
  size_t capacity = 1;
  if (!item) return "";
#define OVERLAY(selector, member) case selector: overlay = item->member; capacity = sizeof item->member; break
  switch (field) {
    OVERLAY(TITLE_ID, imdb); OVERLAY(TITLE_MEDIA_TYPE, tipo); OVERLAY(TITLE_NAME, titulo);
    OVERLAY(TITLE_POSTER, poster); OVERLAY(TITLE_BACKGROUND, backdrop); OVERLAY(TITLE_LOGO, logo);
    OVERLAY(TITLE_GENRE_TEXT, genero); OVERLAY(TITLE_METADATA_TEXT, meta);
    OVERLAY(TITLE_DESCRIPTION, sinopse); OVERLAY(TITLE_COUNTRY, pais); OVERLAY(TITLE_ORIGIN, origem);
    OVERLAY(TITLE_CATALOG_BACKGROUND, backdropCatalogo); OVERLAY(TITLE_TMDB_BACKGROUND, backdropTmdb);
    OVERLAY(TITLE_TRAKT_BACKGROUND, backdropTrakt); OVERLAY(TITLE_LOGO_LANGUAGE, logoIdioma);
    OVERLAY(TITLE_LOGO_LANGUAGE_URL, logoIdiomaUrl); OVERLAY(TITLE_CLASSIFICATION, classificacao);
    OVERLAY(TITLE_DIRECTOR, direcao); OVERLAY(TITLE_SUBSCRIPTION_NAME, provNome);
    OVERLAY(TITLE_SUBSCRIPTION_LOGO, provLogo); OVERLAY(TITLE_RENTAL_NAME, alugNome);
    OVERLAY(TITLE_RENTAL_LOGO, alugLogo); OVERLAY(TITLE_PURCHASE_NAME, compNome);
    OVERLAY(TITLE_PURCHASE_LOGO, compLogo); OVERLAY(TITLE_EPISODE_NAME, nomeEpisodio);
    case TITLE_IMDB_ID: return catalog_title_imdb_id(item);
    default: return "";
  }
#undef OVERLAY
  if (!item->rust_title) return overlay;
  complete = nuvio_title_text(item->rust_title, field);
  if (field == TITLE_ID) return complete; /* Provider identity is immutable. */
  return catalog_text_with_overlay(complete, overlay, capacity);
}
const char *catalog_text_with_overlay(const char *complete, const char *overlay, size_t capacity) {
  size_t length;
  if (!complete || !overlay || !capacity) return "";
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
