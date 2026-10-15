#ifndef NV_CATALOG_ROW_H
#define NV_CATALOG_ROW_H
#include "catalogo.h"
#include "rust_scene.h"
void catalog_row_copy(CatFileira *destination, const CatFileira *source);
void catalog_rows_copy(CatFileira *destination, const CatFileira *source, size_t count);
void catalog_row_clear(CatFileira *row);
void catalog_rows_clear(CatFileira *rows, size_t count);
void catalog_rows_replace(CatFileira *destination, const CatFileira *source, size_t count, size_t capacity);
/* Shared layout caches contain projections only, without borrowed Rust owners. */
void catalog_rows_project(CatFileira *destination, const CatFileira *source, size_t count, size_t capacity);
int catalog_copy_row_index(int index, CatFileira *destination);
int catalog_copy_rows(CatFileira *destination, int capacity);
int catalog_row_from_rust(CatFileira *destination, const RustRowSnapshot *source);
const char *catalog_row_text(const CatFileira *row, unsigned field);
#endif
