#include "profile_context.h"
#include "rust_account.h"
#include "profiles.h"
#include <assert.h>

static uint32_t selected = 1;

/* The retained-test compiler redirects this one query from the C adapter.
 * Production builds and Rust-backed native UI tests never use this fixture. */
uint32_t nuvio_test_active_id(const RustAccount *account) {
  (void)account;
  return selected;
}

void test_select_profile(int id) {
  assert(id > 0);
  selected = (uint32_t)id;
  account_apply_profile();
}
