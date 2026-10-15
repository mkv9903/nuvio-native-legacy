/* No app frame/worker exists in retained C unit fixtures. The real adapter is
 * exercised by the Rust native catalog suite compiled against src/*.c. */
#include "catalog_runtime.h"
void catalog_runtime_init(RustAccount *account, const RustAddons *addons, const char *directory) { (void)account; (void)addons; (void)directory; }
void catalog_runtime_step(void) {}
void catalog_runtime_logout(void) {}
int catalog_runtime_enabled(void) { return 0; }
void catalog_runtime_cycle_begin(void) {}
void catalog_runtime_cycle_end(void) {}
const RustCatalogWork *catalog_runtime_work(void) { return NULL; }
void catalog_runtime_use(const RustCatalogWork *work) { (void)work; }
int catalog_runtime_current(void) { return 1; }
void catalog_runtime_mark_published(void) {}
int catalog_runtime_published_current(void) { return 1; }
int catalog_runtime_display_current(void) { return 1; }
extern int catalog_copy_index(int index, CatItem *title) __attribute__((weak));
int catalog_metadata_copy(int index, CatItem *title) { return catalog_copy_index ? catalog_copy_index(index,title) : 0; }
int catalog_metadata_current(void) { return 1; }
void catalog_metadata_end(void) {}
int catalog_content_begin(void) { return 1; }
void catalog_content_end(void) {}
int catalog_runtime_capture(int save) { (void)save; return 0; }
int catalog_runtime_row_reusable(const CatFileira *row) { (void)row; return 1; }
