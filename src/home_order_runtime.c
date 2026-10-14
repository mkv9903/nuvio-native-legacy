/* Frame publication only; Rust owns fetching, cache ownership and worker I/O. */
#include "home_order_runtime.h"
#include "rust_catalog_order.h"
#include <stdio.h>

static RustHomeOrderController *controller;
void home_order_runtime_init(RustAccount *account, const char *directory) {
  if (!controller) controller = nuvio_home_order_create(account, directory);
  printf("[catalogs] Rust Home-order worker %s\n", controller ? "initialized" : "unavailable");
}
void home_order_runtime_step(int allow_periodic) { nuvio_home_order_step(controller, allow_periodic); }
void home_order_runtime_refresh(void) { nuvio_home_order_refresh(controller); }
void home_order_runtime_logout(void) { nuvio_home_order_logout(controller); }
#ifdef NV_NATIVE_UI_TEST
void home_order_runtime_test_attach(RustHomeOrderController *test_controller) { controller = test_controller; }
#endif
