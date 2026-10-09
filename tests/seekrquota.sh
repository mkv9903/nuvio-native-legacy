#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d /tmp/nuvio-seekrquota.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/data.c tests/seekrquota.c -Isrc -pthread -O1 -g -Wall -Wextra \
  -o /tmp/nuvio-seekrquota-tests
NUVIO_DADOS="$quota_dir" /tmp/nuvio-seekrquota-tests
