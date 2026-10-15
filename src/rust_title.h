#ifndef NV_RUST_TITLE_H
#define NV_RUST_TITLE_H
#include <stddef.h>
#include <stdint.h>
typedef struct RustTitleSnapshot RustTitleSnapshot;
enum RustTitleField {
  TITLE_ID, TITLE_IMDB_ID, TITLE_MEDIA_TYPE, TITLE_NAME, TITLE_POSTER,
  TITLE_BACKGROUND, TITLE_LOGO, TITLE_GENRE_TEXT, TITLE_METADATA_TEXT,
  TITLE_DESCRIPTION, TITLE_COUNTRY, TITLE_ORIGIN,
  TITLE_CATALOG_BACKGROUND, TITLE_TMDB_BACKGROUND, TITLE_TRAKT_BACKGROUND,
  TITLE_LOGO_LANGUAGE, TITLE_LOGO_LANGUAGE_URL, TITLE_CLASSIFICATION, TITLE_DIRECTOR,
  TITLE_SUBSCRIPTION_NAME, TITLE_SUBSCRIPTION_LOGO, TITLE_RENTAL_NAME, TITLE_RENTAL_LOGO,
  TITLE_PURCHASE_NAME, TITLE_PURCHASE_LOGO, TITLE_EPISODE_NAME, TITLE_FIELD_COUNT
};
typedef struct {
  uint32_t percentage, season, episode, remaining_minutes;
  int64_t last_watched_ms;
  int32_t on_watchlist, in_collection;
  size_t cast_count, season_count;
} RustTitleInfo;
/* Decode returns one owned reference, or NULL. Retain creates another owned
 * reference; release consumes one. A text pointer is borrowed from a live
 * snapshot and must not be freed or used after its last reference is released. */
const RustTitleSnapshot *nuvio_title_decode(const char *body, const char *media_type, const char *origin);
const RustTitleSnapshot *nuvio_title_retain(const RustTitleSnapshot *title);
void nuvio_title_release(const RustTitleSnapshot *title);
/* Retirement consumes an owned reference and keeps frame readers alive across
 * two frame advances. Call next_frame once at the catalog frame boundary. */
void nuvio_title_retire(const RustTitleSnapshot *title);
void nuvio_titles_next_frame(void);
const char *nuvio_title_text(const RustTitleSnapshot *title, unsigned field);
void nuvio_title_copy_text(const RustTitleSnapshot *title, unsigned field, char *output, size_t capacity);
int nuvio_title_rating(const RustTitleSnapshot *title);
int nuvio_title_rating_value(double value);
uint64_t nuvio_title_tmdb_id(const RustTitleSnapshot *title);
/* Info, cast text and all title text remain borrowed from a retained title.
 * Cast selectors: 0=name, 1=role, 2=photo. Counts are not C display limits. */
const RustTitleInfo *nuvio_title_info(const RustTitleSnapshot *title);
const char *nuvio_title_cast_text(const RustTitleSnapshot *title, size_t index, unsigned field);
void nuvio_title_copy_cast_text(const RustTitleSnapshot *title, size_t index, unsigned field, char *output, size_t capacity);
uint64_t nuvio_title_cast_tmdb_id(const RustTitleSnapshot *title, size_t index);
uint32_t nuvio_title_season(const RustTitleSnapshot *title, size_t index);
#endif
