#!/bin/bash
# Provider-styled letters fold to ordinary glyphs (#144).
set -euo pipefail
cd "$(dirname "$0")/.."
source tests/text_helpers_build.sh
cc "${text_flags[@]}" src/dobra.c tests/dobra.c "${text_libraries[@]}" -o "$text_scratch/test"
"$text_scratch/test"
