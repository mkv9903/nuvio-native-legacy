/* Ownership and display projections of complete Rust catalog row records. */
#include "catalog_row.h"
#include "catalog_item.h"
#include <string.h>
#include <limits.h>
#include <stdlib.h>
void catalog_row_copy(CatFileira *destination, const CatFileira *source) {
  if (!destination || !source || destination == source) return;
  *destination = *source; destination->rust_row = nuvio_row_retain(source->rust_row);
}
void catalog_row_clear(CatFileira *row) {
  if (!row) return;
  nuvio_row_release(row->rust_row); memset(row, 0, sizeof *row);
}
void catalog_rows_copy(CatFileira *destination, const CatFileira *source, size_t count) {
  size_t i; for (i=0;i<count;i++) catalog_row_copy(destination+i,source+i);
}
void catalog_rows_clear(CatFileira *rows, size_t count) {
  size_t i; for (i=0;rows&&i<count;i++) catalog_row_clear(rows+i);
}
void catalog_rows_replace(CatFileira *destination, const CatFileira *source, size_t count, size_t capacity) {
  size_t index;
  if (count>capacity || (count && !source)) return;
  if (destination==source) { catalog_rows_clear(destination+count,capacity-count); return; }
  for (index=0;index<count;index++) nuvio_row_retain(source[index].rust_row);
  for (index=0;index<capacity;index++) nuvio_row_release(destination[index].rust_row);
  if (count) memmove(destination,source,count*sizeof *destination);
  memset(destination+count,0,(capacity-count)*sizeof *destination);
}
int catalog_row_from_rust(CatFileira *destination, const RustRowSnapshot *source) {
  const RustRowInfo *info=nuvio_row_info(source);
  if (!destination || !info || info->start>INT_MAX || info->end>INT_MAX) return 0;
  memset(destination,0,sizeof *destination); destination->rust_row=source;
#define FIELD(selector,member) nuvio_row_copy_text(source,selector,destination->member,sizeof destination->member)
  FIELD(ROW_KEY,chave); FIELD(ROW_NAME,titulo); FIELD(ROW_MEDIA_TYPE,tipo);
  FIELD(ROW_ADDON_URL,base); FIELD(ROW_CATALOG_ID,catId);
#undef FIELD
  destination->ini=(int)info->start; destination->n=(int)(info->end-info->start);
  destination->estado=info->state==ROW_CONFIRMED_EMPTY;
  return 1;
}
void catalog_rows_project(CatFileira *destination, const CatFileira *source, size_t count, size_t capacity) {
  size_t index;
  if (count>capacity || (count && !source)) return;
  for (index=0;index<capacity;index++) nuvio_row_release(destination[index].rust_row);
  if (count) memmove(destination,source,count*sizeof *destination);
  for (index=0;index<count;index++) destination[index].rust_row=NULL;
  memset(destination+count,0,(capacity-count)*sizeof *destination);
}
const char *catalog_row_text(const CatFileira *row, unsigned field) {
  const char *overlay=""; size_t capacity=1;
  if (!row) return "";
#define FIELD(selector,member) case selector: overlay=row->member; capacity=sizeof row->member; break
  switch (field) {
    FIELD(ROW_KEY,chave); FIELD(ROW_NAME,titulo); FIELD(ROW_MEDIA_TYPE,tipo);
    FIELD(ROW_ADDON_URL,base); FIELD(ROW_CATALOG_ID,catId);
    default: return row->rust_row ? nuvio_row_text(row->rust_row,field) : "";
  }
#undef FIELD
  return row->rust_row ? catalog_text_with_overlay(nuvio_row_text(row->rust_row,field),overlay,capacity) : overlay;
}
