#ifndef NV_CATALOG_ITEM_H
#define NV_CATALOG_ITEM_H
#include "catalogo.h"
/* Copy creates an owned reference in an uninitialized destination. Replace
 * releases an initialized destination. Move transfers ownership and zeros it. */
void catalog_item_copy(CatItem *destination, const CatItem *source);
void catalog_item_replace(CatItem *destination, const CatItem *source);
void catalog_item_move(CatItem *destination, CatItem *source);
void catalog_item_clear(CatItem *item);
void catalog_items_copy(CatItem *destination, const CatItem *source, size_t count);
void catalog_items_clear(CatItem *items, size_t count);
/* Takes ownership of the Rust snapshot; destination must be uninitialized. */
int catalog_item_from_rust(CatItem *destination, const RustTitleSnapshot *title);
/* Complete Rust text when the legacy overlay still matches its projection. */
const char *catalog_title_text(const CatItem *item, unsigned field);
const char *catalog_title_id(const CatItem *item);
const char *catalog_title_imdb_id(const CatItem *item);
/* Read the complete value if its bounded display projection is unchanged. */
const char *catalog_text_with_overlay(const char *complete, const char *overlay, size_t capacity);
#endif
