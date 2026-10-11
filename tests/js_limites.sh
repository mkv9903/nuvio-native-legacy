#!/bin/bash
# Explicit ranges, incomplete input, fallback preservation and root lookup.
set -euo pipefail
cd "$(dirname "$0")/.."
export SANITIZE="${SANITIZE:-1}"
source tests/json_helpers_build.sh
cc "${json_flags[@]}" src/js.c tests/js_limites.c "${json_libraries[@]}" -o "$json_scratch/bounds"
"$json_scratch/bounds"
