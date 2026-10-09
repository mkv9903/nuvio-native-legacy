#!/bin/bash
# Saude da rede para a ilha (sem internet / de volta). Sem rede; ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc tests/redesaude.c src/net_health.c -Isrc -o /tmp/nuvio-redesaude -lpthread
/tmp/nuvio-redesaude
