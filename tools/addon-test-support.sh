#!/usr/bin/env bash
# Retained C algorithms consume real Rust data in these test fixtures.
test_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export test_root
test_backend_target=${CARGO_TARGET_DIR:-$test_root/build/backend/target}
export CARGO_TARGET_DIR="$test_backend_target"
CARGO_TARGET_DIR="$test_backend_target" cargo build --locked --manifest-path "$test_root/backend/Cargo.toml" --features native-ui-tests || return
export NUVIO_TEST_BACKEND_LIB="$test_backend_target/debug/libnuvio_backend.a"
cc() {
  local registry=0 adapter=0 statistics=0 needs_items=0 has_items=0 argument
  local fixture_flags=(-DNV_NATIVE_UI_TEST "$test_root/tests/support/addon_context.c")
  for argument in "$@"; do
    case "$argument" in *src/addon_registry.c) registry=1 ;; *src/addon_registry_adapter.c) adapter=1 ;; esac
    case "$argument" in *src/addonstats.c) statistics=1 ;; esac
    case "$argument" in *src/catalogo.c|*src/descoberta.c|*src/trakt.c) needs_items=1 ;; *src/catalog_item.c) has_items=1 ;; esac
  done
  if [ "$registry" = 0 ]; then fixture_flags+=("$test_root/src/addon_registry.c"); fi
  if [ "$adapter" = 0 ]; then fixture_flags+=("$test_root/src/addon_registry_adapter.c"); fi
  if [ "$statistics" = 0 ]; then fixture_flags+=("$test_root/src/addonstats.c" "$test_root/tests/support/addon_statistics_storage.c"); fi
  if [ "$needs_items" = 1 ] && [ "$has_items" = 0 ]; then fixture_flags+=("$test_root/src/catalog_item.c"); fi
  command "${NUVIO_TEST_CC:-cc}" "$@" "${fixture_flags[@]}" "$NUVIO_TEST_BACKEND_LIB" -ldl -pthread -lm
}
export -f cc
