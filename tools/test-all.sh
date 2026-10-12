#!/bin/bash
# Runs all lightweight tests from tests/, skipping non-gatekeeper benchmarks:
#   *_shot / cinematic / director  -> require artwork and GL, not logic tests
#   fluidez_perf_player            -> player frame time benchmark
#   fluidez_perf_guia              -> TV guide frame time benchmark
#   salvospainel_perf              -> per-frame metric to compare tree structures;
#                                     does not pass/fail (gate is salvospainel.sh)
cd "$(dirname "$0")/.." || exit 1
source tools/test-support.sh || exit 1
LOGS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-testes.XXXXXX") || exit 1
failed=0
passed=0
skipped=0
noProof=0
for f in tests/*.sh; do
  n=$(basename "$f")
  case "$n" in
    *_shot.sh|cinematic.sh|director.sh|salvospainel_perf.sh|fluidez_perf_player.sh|fluidez_perf_guia.sh) skipped=$((skipped + 1)); continue;;
  esac
  if bash "$f" >"$LOGS/$n.log" 2>&1; then
    # SKIP exits with 0 but proves nothing: do not count it as passed.
    if grep -Eq '(^|[^A-Za-z])SKIP([^A-Za-z]|$)' "$LOGS/$n.log"; then
      echo "SKIP  $n (UNVERIFIED: $(grep -Em1 'SKIP' "$LOGS/$n.log" | cut -c1-90))"
      noProof=$((noProof + 1)); continue
    fi
    if grep -Eq 'PULADO|pulados|sem resultado|SKIPPED' "$LOGS/$n.log"; then
      echo "ok*   $n (see skipped cases in log)"
    else
      echo "ok    $n"
    fi
    passed=$((passed + 1))
  else
    echo "FAIL  $n"; tail -15 "$LOGS/$n.log"; failed=$((failed + 1))
  fi
done
echo "tests: $passed passed, $failed failed, $noProof SKIP unverified, $skipped outside suite; logs: $LOGS"
[ "$failed" -eq 0 ]
