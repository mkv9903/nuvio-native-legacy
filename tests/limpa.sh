#!/bin/bash
# Bundled-font corpus, separators and UTF-8 truncation (#144).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${SANITIZE:=1}"
source tests/text_helpers_build.sh
python3 tools/glifos.py > "$text_scratch/inter.txt"
cc "${text_flags[@]}" src/limpa.c src/dobra.c tests/limpa.c "${text_libraries[@]}" -o "$text_scratch/test"
"$text_scratch/test" "$text_scratch/inter.txt" tests/limpa_corpus.txt
