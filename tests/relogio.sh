#!/bin/bash
# Relogio interpolado da legenda ASS (#92). Ver tests/relogio.c.
set -eu
cd "$(dirname "$0")/.."
cc -Wall -O1 tests/relogio.c src/clock.c -o /tmp/nuvio-relogio-test -lm
/tmp/nuvio-relogio-test
