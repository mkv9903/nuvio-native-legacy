#!/usr/bin/env bash
# Link retained C capability tests to the Rust backend when they build the app.
# Test assertions remain in the appropriate language for their capability.
test_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export test_root
if [ -z "${NUVIO_TEST_BACKEND_LIB:-}" ]; then
  test_backend_target=${CARGO_TARGET_DIR:-$test_root/build/backend/target}
  bash "$test_root/tools/backend.sh" --build || return
  export NUVIO_TEST_BACKEND_LIB="$test_backend_target/debug/libnuvio_backend.a"
fi
cc() {
  local has_backend=0 profile_fixture=0 needs_registry=0 has_registry=0 argument
  local fixture_flags=()
  for argument in "$@"; do
    case "$argument" in *src/account_ui_adapter.c|*src/addon_registry.c|*src/addon_registry_adapter.c|*src/descoberta.c|*src/sync.c|*src/catalog_order_adapter.c|*src/catalog_order_cache_adapter.c) has_backend=1 ;; esac
    case "$argument" in *src/account_ui_adapter.c|*src/sync.c) needs_registry=1 ;; esac
    case "$argument" in *src/addon_registry.c) has_registry=1 ;; esac
    case "$argument" in tests/listas.c|tests/salvospainel_tabs.c|tests/player_retido.c|tests/releaseinfo.c) profile_fixture=1 ;; esac
  done
  if [ "$has_backend" = 1 ]; then
    if [ "$needs_registry" = 1 ] && [ "$has_registry" = 0 ]; then fixture_flags+=("$test_root/src/addon_registry.c"); fi
    if [ "$profile_fixture" = 1 ]; then
      fixture_flags+=(tests/support/profile_context.c -Dnuvio_account_active_id=nuvio_test_active_id)
    fi
    # Retained C unit fixtures already supply and verify temporary data roots.
    # Keep their existing directory contract; real app/UI tests compile the
    # isolated Rust Test identity and exercise actual Rust profile actions.
    command "${NUVIO_TEST_CC:-cc}" "$@" "${fixture_flags[@]}" "$NUVIO_TEST_BACKEND_LIB" -ldl -pthread -lm
  else
    command "${NUVIO_TEST_CC:-cc}" "$@"
  fi
}
export -f cc
