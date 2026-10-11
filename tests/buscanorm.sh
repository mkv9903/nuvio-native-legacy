#!/bin/bash
# Local search folding, UTF-8 boundaries and backspace (#176).
set -euo pipefail
cd "$(dirname "$0")/.."
source tests/text_helpers_build.sh
cc "${text_flags[@]}" tests/buscanorm.c src/buscanorm.c "${text_libraries[@]}" -o "$text_scratch/test"
"$text_scratch/test"
