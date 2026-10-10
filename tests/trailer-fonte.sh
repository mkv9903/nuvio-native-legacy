#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -DNV_LINUX_DESKTOP -Isrc $(pkg-config --cflags sdl2) tests/trailer-fonte.c src/trailerfonte.c -o /tmp/nuvio-trailer-fonte
/tmp/nuvio-trailer-fonte
