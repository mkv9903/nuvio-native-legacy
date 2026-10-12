#include "addon_context.h"
#include "addon_registry.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern void *nuvio_test_addon_fixture_create(const char *directory);
extern RustAddons *nuvio_test_addon_fixture_handle(void *fixture);
extern int nuvio_test_addon_fixture_replace(void *fixture, const char *json);
extern void nuvio_test_addon_fixture_destroy(void *fixture);
static void *fixture;

void test_addons_set(const char *json) {
  if (!fixture) {
    char parent[] = "/tmp/nuvio-native-addon-fixture-XXXXXX";
    char directory[512];
    assert(mkdtemp(parent));
    snprintf(directory, sizeof directory, "%s/space.mkv.nuvio.rusttest", parent);
    fixture = nuvio_test_addon_fixture_create(directory);
    assert(fixture);
    addon_registry_test_attach(nuvio_test_addon_fixture_handle(fixture));
  }
  assert(nuvio_test_addon_fixture_replace(fixture, json));
  addon_registry_refresh();
}
void test_addons_release(void) {
  addon_registry_test_attach(NULL);
  nuvio_test_addon_fixture_destroy(fixture);
  fixture = NULL;
}
