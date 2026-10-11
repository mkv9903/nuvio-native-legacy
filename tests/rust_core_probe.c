#include "rust_core.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  NuvioRustProbe report = {0};
  char missing[1024];
  assert(argc == 2);
  assert(sizeof report == 16 && offsetof(NuvioRustProbe, checksum) == 8);
  assert(nuvio_rust_core_abi_version() == NUVIO_RUST_ABI_VERSION);
  assert(nuvio_rust_core_probe(NULL, &report) == 1);
  assert(nuvio_rust_core_probe(argv[1], NULL) == 1);
  assert(nuvio_rust_core_probe("", &report) == 1);
  assert(nuvio_rust_core_probe(argv[1], &report) == 0);
  assert(report.abi_version == NUVIO_RUST_ABI_VERSION);
  assert(report.checks == NUVIO_RUST_CHECKS_OK);
  assert(report.checksum == NUVIO_RUST_CHECKSUM);
  snprintf(missing, sizeof missing, "%s/nonexistent/probe", argv[1]);
  assert(nuvio_rust_core_probe(missing, &report) == 3);
  assert(report.checks == (NUVIO_RUST_MEMORY_OK | NUVIO_RUST_THREAD_OK));
  printf("Rust/C probe: PASS (ABI, allocation, thread, file, invalid input)\n");
  return 0;
}
