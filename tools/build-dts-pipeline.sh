#!/usr/bin/env bash
# ARM adapters loaded dynamically by the main app.
# Default outputs are ignored build artifacts; arm.sh stages them into deploy/app/lib.
set -euo pipefail
cd "$(dirname "$0")/.."
output="${NUVIO_DTS_PIPELINE_OUTPUT:-build/dts-pipeline}"
inside=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --in-container) inside=1; shift ;;
    --output) [ "$#" -gt 1 ] || { echo '--output needs a directory' >&2; exit 2; }; output=$2; shift 2 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
done
mkdir -p "$output"
if [ "$inside" = 0 ]; then
  runtime="${NUVIO_CONTAINER_RUNTIME:-podman}"
  image="${NUVIO_DTS_SDK_IMAGE:-localhost/nuvio-webos-sdk:latest}"
  output=$(cd "$output" && pwd)
  exec "$runtime" run --rm -v "$PWD:/work" -v "$output:/dts-out" -w /work "$image" \
    bash tools/build-dts-pipeline.sh --in-container --output /dts-out
fi
cxx="${NUVIO_DTS_CXX:-/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin/arm-webos-linux-gnueabi-g++}"
cc="${NUVIO_DTS_CC:-/opt/arm-webos-linux-gnueabi_sdk-buildroot/bin/arm-webos-linux-gnueabi-gcc}"
strip_tool="${NUVIO_DTS_STRIP:-}"
if [ -z "$strip_tool" ]; then
  case "$cxx" in
    *-g++) strip_tool="${cxx%-g++}-strip" ;;
    *g++)  strip_tool="${cxx%g++}strip" ;;
  esac
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
"$cc" -std=c11 -D_GNU_SOURCE -O2 -ffunction-sections -fdata-sections -fPIC -Isrc -c src/js.c -o "$tmp/js.o"
for abi in 0 1; do
  generation=3
  if [ "$abi" = 1 ]; then generation=4; fi
  "$cxx" -std=c++11 -O2 -ffunction-sections -fdata-sections -fPIC -shared -D_GLIBCXX_USE_CXX11_ABI=$abi \
    -Isrc -Isrc/dts/adapter src/dts/adapter/starfish.cpp "$tmp/js.o" \
    -Wl,--gc-sections -o "$output/dts-starfish-webos$generation.so" -ldl -pthread
  if [ -n "$strip_tool" ] && command -v "$strip_tool" >/dev/null 2>&1; then
    "$strip_tool" --strip-unneeded "$output/dts-starfish-webos$generation.so"
  fi
done
