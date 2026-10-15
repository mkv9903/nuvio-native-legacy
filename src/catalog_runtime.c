/* Frame publication and job ownership only; Rust owns catalog I/O and storage. */
#include "catalog_runtime.h"
#include "catalog_item.h"
#include "catalog_row.h"
#include "home_focus.h"
#include "addon_registry.h"
#include "descoberta.h"
#include <pthread.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static const RustCatalogRuntime *runtime;
static int initialized;
static pthread_key_t work_key;
static pthread_once_t work_once = PTHREAD_ONCE_INIT;
static uint64_t last_revision, last_activation;
static int has_activation;
static const RustSceneSnapshot *last_scene;
static pthread_mutex_t publication_lock = PTHREAD_MUTEX_INITIALIZER;
static const RustCatalogWork *published_work;
static pthread_mutex_t frame_lock = PTHREAD_MUTEX_INITIALIZER;
static RustCatalogSceneView *display_view;
static pthread_key_t metadata_key;
static pthread_once_t metadata_once = PTHREAD_ONCE_INIT;
static pthread_key_t content_key;
static pthread_once_t content_once = PTHREAD_ONCE_INIT;
static void release_content(void *lease) { nuvio_catalog_content_end(lease); }
static void create_content_key(void) { pthread_key_create(&content_key,release_content); }
void catalog_content_end(void) {
  pthread_once(&content_once,create_content_key);
  nuvio_catalog_content_end(pthread_getspecific(content_key)); pthread_setspecific(content_key,NULL);
}
int catalog_content_begin(void) {
  RustContentLease *lease;
  catalog_content_end();
  if (!initialized) return 1;
  lease=nuvio_catalog_content_begin(runtime); pthread_setspecific(content_key,lease);
  return lease!=NULL;
}
static void release_metadata(void *view) { nuvio_catalog_view_release(view); }
static void create_metadata_key(void) { pthread_key_create(&metadata_key,release_metadata); }
void catalog_metadata_end(void) {
  pthread_once(&metadata_once,create_metadata_key);
  nuvio_catalog_view_release(pthread_getspecific(metadata_key));
  pthread_setspecific(metadata_key,NULL);
}
int catalog_metadata_current(void) {
  RustCatalogSceneView *view;
  RustContentLease *content;
  if (!initialized) return 1;
  if (!runtime) return 0;
  pthread_once(&metadata_once,create_metadata_key);
  view=pthread_getspecific(metadata_key);
  pthread_once(&content_once,create_content_key);
  content=pthread_getspecific(content_key);
  if (!view && content) return nuvio_catalog_content_current(content);
  return view ? nuvio_catalog_view_context_current(view) : (!catalog_runtime_work() || catalog_runtime_current());
}
int catalog_runtime_display_current(void) {
  int valid;
  pthread_mutex_lock(&frame_lock);
  valid=!initialized || (runtime && nuvio_catalog_view_context_current(display_view));
  pthread_mutex_unlock(&frame_lock); return valid;
}
int catalog_metadata_copy(int index, CatItem *title) {
  RustCatalogSceneView *view=NULL;
  RustAddonSnapshot *addons=NULL;
  int found=0;
  catalog_metadata_end();
  pthread_mutex_lock(&frame_lock);
  if (!initialized || (runtime && nuvio_catalog_view_context_current(display_view))) {
    view=nuvio_catalog_view_clone(display_view);
    found=catalog_copy_index(index,title);
    if (found && initialized) {
      addons=nuvio_catalog_view_addons(view);
      if (!addons) { catalog_item_clear(title); found=0; }
    }
  }
  pthread_mutex_unlock(&frame_lock);
  if (found) {
    pthread_setspecific(metadata_key,view);
    if (addons) addon_registry_use(addons);
  }
  else nuvio_catalog_view_release(view);
  nuvio_addons_snapshot_destroy(addons);
  return found;
}
#ifdef NV_NATIVE_UI_TEST
static int test_attached;
void catalog_runtime_test_attach(const RustCatalogRuntime *test_runtime) {
  runtime=test_runtime; last_revision=0; last_activation=0; has_activation=0; test_attached=1;
  initialized=test_runtime!=NULL;
  nuvio_scene_release(last_scene); last_scene=NULL;
  nuvio_catalog_view_release(display_view); display_view=NULL;
  catalog_metadata_end();
  catalog_content_end();
  catalog_runtime_cycle_end();
  pthread_mutex_lock(&publication_lock);
  nuvio_catalog_work_release(published_work); published_work=NULL;
  pthread_mutex_unlock(&publication_lock);
}
#endif
int catalog_runtime_row_reusable(const CatFileira *row) {
  if (!runtime || !row || !row->base[0]) return 1;
  return nuvio_catalog_row_reusable(catalog_runtime_work(), catalog_row_text(row, ROW_ADDON_URL), catalog_row_text(row, ROW_MEDIA_TYPE), catalog_row_text(row, ROW_CATALOG_ID));
}
void catalog_runtime_mark_published(void) {
  const RustCatalogWork *work = nuvio_catalog_work_retain(catalog_runtime_work());
  pthread_mutex_lock(&publication_lock);
  nuvio_catalog_work_release(published_work); published_work=work;
  pthread_mutex_unlock(&publication_lock);
}
int catalog_runtime_published_current(void) {
  int current;
  if (!runtime) return 1;
  pthread_mutex_lock(&publication_lock);
  current=nuvio_catalog_work_current(published_work);
  pthread_mutex_unlock(&publication_lock); return current;
}
static void release_work(void *work) { nuvio_catalog_work_release(work); }
static void create_work_key(void) { pthread_key_create(&work_key, release_work); }
const RustCatalogWork *catalog_runtime_work(void) {
  pthread_once(&work_once, create_work_key);
  return pthread_getspecific(work_key);
}
void catalog_runtime_use(const RustCatalogWork *work) {
  const RustCatalogWork *owned = nuvio_catalog_work_retain(work);
  const RustCatalogWork *old = catalog_runtime_work();
  pthread_setspecific(work_key, owned);
  nuvio_catalog_work_release(old);
}
void catalog_runtime_cycle_begin(void) {
  pthread_mutex_lock(&frame_lock);
  const RustCatalogWork *work = nuvio_catalog_begin(runtime);
  catalog_runtime_use(work); nuvio_catalog_work_release(work);
  pthread_mutex_unlock(&frame_lock);
}
void catalog_runtime_cycle_end(void) { catalog_runtime_use(NULL); }
int catalog_runtime_enabled(void) { return initialized; }
int catalog_runtime_current(void) { return !initialized || (runtime && nuvio_catalog_work_current(catalog_runtime_work())); }
void catalog_runtime_init(RustAccount *account, const RustAddons *addons, const char *directory) {
  initialized=1;
  if (!runtime) runtime = nuvio_catalog_create(account, addons, directory);
  printf("[catalogs] Rust catalog loader and scene store %s\n", runtime ? "initialized" : "unavailable");
}
int catalog_runtime_capture(int save) {
  const RustSceneSnapshot *scene;
  int result;
  if (!runtime || !catalog_runtime_current()) return 0;
  scene = catalog_capture_scene();
  result = nuvio_catalog_accept(catalog_runtime_work(), scene, save);
  nuvio_scene_release(scene);
  return result;
}
static void step_frame(void) {
  RustCatalogSceneView *view;
  const RustSceneSnapshot *scene;
  uint64_t revision, activation;
  int changed;
  unsigned catalog_revision;
  if (!runtime) return;
  nuvio_catalog_step(runtime);
  revision = nuvio_catalog_revision(runtime);
  if (revision == last_revision) return;
  catalog_revision = cat_revisao();
  view = nuvio_catalog_view(runtime);
  if (!view || !nuvio_catalog_view_current(view)) { nuvio_catalog_view_release(view); return; }
  activation = nuvio_catalog_view_activation(view);
  changed = has_activation && activation != last_activation;
  scene = nuvio_catalog_view_scene(view);
  if (!has_activation || activation != last_activation || (nuvio_catalog_view_requires_apply(view) && scene != last_scene)) {
    if (nuvio_catalog_view_current(view)) {
      if (!has_activation || changed) discovery_metadata_context_changed();
      if (catalog_apply_scene_view(scene, 1, view, catalog_revision)) {
        pthread_mutex_lock(&publication_lock);
        nuvio_catalog_work_release(published_work); published_work=NULL;
        pthread_mutex_unlock(&publication_lock);
        if (!has_activation || changed) home_clear_focus();
        last_activation = activation; has_activation = 1;
        printf("[catalogs] profile scene %s: %zu titles, %zu rows\n", scene ? "restored" : "loading", nuvio_scene_title_count(scene), nuvio_scene_row_count(scene));
        if (changed
#ifdef NV_NATIVE_UI_TEST
            && !test_attached
#endif
        ) desc_repetir_silencioso();
      } else { nuvio_scene_release(scene); nuvio_catalog_view_release(view); return; }
    }
  }
  last_revision = revision;
  nuvio_scene_release(last_scene); last_scene=scene;
  nuvio_catalog_view_release(display_view); display_view=view;
}
void catalog_runtime_step(void) { pthread_mutex_lock(&frame_lock); step_frame(); pthread_mutex_unlock(&frame_lock); }
void catalog_runtime_logout(void) { nuvio_catalog_logout(runtime); }
