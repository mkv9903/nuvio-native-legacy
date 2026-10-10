#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp /tmp/nuvio-platform-dispatch.XXXXXX)
trap 'rm -f "$binary"' EXIT
check() {
  local expected=$1 name=$2
  shift 2
  cc tests/diagnostico_dispatch.c src/perfiltv.c -Isrc -Wall -Wextra \
    "-DTEST_PTV_EXPECTED=$expected" "-DTEST_PTV_NAME=\"$name\"" "$@" -o "$binary"
  "$binary"
}
check PTV_LG lg
check PTV_LG lg -DNV_WEBOS
check PTV_LG lg -DNV_LINUX_DESKTOP
