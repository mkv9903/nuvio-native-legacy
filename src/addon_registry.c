/* Stable native reads of Rust-owned addon state. No registry, parsing, saved
 * edits, networking or fixed provider buffers are implemented in this adapter. */
#include "addon_registry.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RustAddons *controller;
static pthread_key_t reader_key;
static pthread_once_t reader_once = PTHREAD_ONCE_INIT;
static const RustAddonSnapshotView empty_view = { .status = RUST_ADDONS_UNAVAILABLE, .error = "" };
typedef struct { RustAddonSnapshot *snapshot; const RustAddonSnapshotView *view; } Reader;

static void release_reader(void *pointer) {
  Reader *reader = pointer;
  if (!reader) return;
  nuvio_addons_snapshot_destroy(reader->snapshot);
  free(reader);
}
static void create_reader_key(void) { pthread_key_create(&reader_key, release_reader); }
static Reader *get_reader(void) {
  Reader *reader;
  pthread_once(&reader_once, create_reader_key);
  reader = pthread_getspecific(reader_key);
  if (!reader) {
    reader = calloc(1, sizeof *reader);
    if (reader) pthread_setspecific(reader_key, reader);
  }
  return reader;
}
static void take_snapshot(Reader *reader, RustAddonSnapshot *snapshot) {
  if (!reader) { nuvio_addons_snapshot_destroy(snapshot); return; }
  nuvio_addons_snapshot_destroy(reader->snapshot);
  reader->snapshot = snapshot;
  reader->view = nuvio_addons_snapshot_view(snapshot);
}
void addon_registry_refresh(void) {
  Reader *reader = get_reader();
  if (!reader) return;
  if (controller && reader->view && reader->view->revision == nuvio_addons_revision(controller)) return;
  take_snapshot(reader, nuvio_addons_snapshot(controller));
}
void addon_registry_init(RustAccount *account, const char *directory) {
  /* Called once after the account capability and test data directory exist. */
  if (!controller) controller = nuvio_addons_create(account, directory);
  addon_registry_refresh();
}
void addon_registry_step(int force_primary, int allow_periodic) {
  static uint64_t logged_list;
  static int logged_status = -1, logged_pending = -1;
  const RustAddonSnapshotView *view;
  nuvio_addons_step_controlled(controller, force_primary, allow_periodic);
  addon_registry_refresh();
  view = addon_registry_view();
  if (controller && (logged_list != view->list_revision || logged_status != view->status || logged_pending != view->pending)) {
    printf("[addons] Rust state: profile=%u addon_profile=%u count=%zu status=%d pending=%d\n",
           view->profile, view->addon_profile, view->count, view->status, view->pending);
    logged_list = view->list_revision; logged_status = view->status; logged_pending = view->pending;
  }
}
void addon_registry_use(const RustAddonSnapshot *snapshot) {
  take_snapshot(get_reader(), nuvio_addons_snapshot_clone(snapshot));
}
RustAddonSnapshot *addon_registry_capture(void) {
  Reader *reader = get_reader();
  if (reader && !reader->view) addon_registry_refresh();
  return reader ? nuvio_addons_snapshot_clone(reader->snapshot) : NULL;
}
const RustAddonSnapshotView *addon_registry_view(void) {
  Reader *reader = get_reader();
  if (reader && !reader->view) addon_registry_refresh();
  return reader && reader->view ? reader->view : &empty_view;
}
const RustAddonView *addon_registry_items(void) { return addon_registry_view()->addons; }
int addon_registry_count(void) {
  size_t count = addon_registry_view()->count;
  return count > INT_MAX ? INT_MAX : (int)count;
}
unsigned addon_registry_version(void) { return (unsigned)nuvio_addons_list_revision(controller); }
int addon_registry_snapshot_current(const RustAddonSnapshot *snapshot) { return nuvio_addons_snapshot_current(controller, snapshot); }
int addon_registry_reader_current(void) { Reader *reader = get_reader(); return reader && addon_registry_snapshot_current(reader->snapshot); }
void addon_registry_request_refresh(void) { nuvio_addons_refresh(controller); }
void addon_registry_logout(void) { nuvio_addons_logout(controller); addon_registry_refresh(); }
int addon_registry_toggle(int index) {
  Reader *reader = get_reader();
  const RustAddonSnapshotView *view = addon_registry_view();
  int enabled;
  if (!reader || index < 0 || (size_t)index >= view->count) return -1;
  enabled = !view->addons[index].enabled;
  if (!nuvio_addons_set_enabled(controller, reader->snapshot, (size_t)index, enabled)) return -1;
  addon_registry_refresh();
  return enabled;
}
int addon_registry_install(const char *name, const char *url) {
  Reader *reader = get_reader();
  addon_registry_view();
  if (!reader || !nuvio_addons_install(controller, reader->snapshot, name ? name : "Addon", url)) return 0;
  addon_registry_refresh();
  return 1;
}
int addon_registry_attach_manifest(int index, const char *body) {
  Reader *reader = get_reader();
  return reader && index >= 0 && nuvio_addons_attach_manifest(controller, reader->snapshot, (size_t)index, body);
}
int64_t addon_registry_report_response(int index, int responded) {
  Reader *reader = get_reader();
  return reader && index >= 0 ? nuvio_addons_report_response(controller, reader->snapshot, (size_t)index, responded) : -1;
}
char *addon_registry_manifest_url(int index) {
  Reader *reader = get_reader();
  return reader && index >= 0 ? nuvio_addons_snapshot_manifest_url(reader->snapshot, (size_t)index) : NULL;
}
char *addon_registry_resource_url(int index, const char *resource, const char *type, const char *id, const char *extras) {
  Reader *reader = get_reader();
  if (!reader || index < 0) return NULL;
  return extras && *extras
    ? nuvio_addons_snapshot_url_encoded_extras(reader->snapshot, (size_t)index, resource, type, id, extras)
    : nuvio_addons_snapshot_url(reader->snapshot, (size_t)index, resource, type, id);
}

#ifdef NV_NATIVE_UI_TEST
void addon_registry_test_attach(RustAddons *test_controller) {
  take_snapshot(get_reader(), NULL);
  controller = test_controller;
  addon_registry_refresh();
}
#endif
