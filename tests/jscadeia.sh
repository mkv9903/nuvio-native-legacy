#!/bin/bash
# Standalone JSON strings, Unicode escapes and caller-owned output buffers.
set -euo pipefail
cd "$(dirname "$0")/.."
source tests/json_helpers_build.sh
cc "${json_flags[@]}" src/js.c tests/jscadeia.c "${json_libraries[@]}" -o "$json_scratch/strings"
"$json_scratch/strings"
