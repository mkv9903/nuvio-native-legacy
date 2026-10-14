#ifndef NV_RUST_TITLE_H
#define NV_RUST_TITLE_H
#include <stddef.h>
#include <stdint.h>
typedef struct RustTitleSnapshot RustTitleSnapshot;
enum RustTitleField {
  TITLE_ID, TITLE_IMDB_ID, TITLE_MEDIA_TYPE, TITLE_NAME, TITLE_POSTER,
  TITLE_BACKGROUND, TITLE_LOGO, TITLE_GENRE_TEXT, TITLE_METADATA_TEXT,
  TITLE_DESCRIPTION, TITLE_COUNTRY, TITLE_ORIGIN
};
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
uint64_t nuvio_title_tmdb_id(const RustTitleSnapshot *title);
#endif
