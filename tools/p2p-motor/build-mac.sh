#!/bin/bash
# nuvio-engine (pinned commit) + libtorrent on Mac, for PoC and for
# tests/p2pmotor_real.sh. Requires `brew install cmake openssl@3`.
#
#   bash tools/p2p-motor/build-mac.sh [folder]     # default /tmp/nv-p2p-motor-mac
#   NV_ENGINE=<folder>/nuvio-engine bash tests/p2pmotor_real.sh
set -eu
cd "$(dirname "$0")/../.."
W="${1:-/tmp/nv-p2p-motor-mac}"
mkdir -p "$W"
[ -d "$W/nuvio-engine/.git" ] || git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
git -C "$W/nuvio-engine" checkout -q 02938d7
cmake -S "$W/nuvio-engine" -B "$W/nuvio-engine/build/mac" -DCMAKE_BUILD_TYPE=Release \
  -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON -DNUVIO_ENGINE_BUILD_TESTS=OFF \
  -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)" >/dev/null
cmake --build "$W/nuvio-engine/build/mac" -j8 >/dev/null
O=$(brew --prefix openssl@3); E="$W/nuvio-engine"
clang++ -O2 -x c tools/p2p-motor/poc.c -x none -I"$E/include" "$E/build/mac/libnuvio_engine.a" \
  "$E/build/mac/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a" "$O/lib/libssl.a" "$O/lib/libcrypto.a" \
  -framework Security -framework CoreFoundation -framework SystemConfiguration -o "$W/poc-mac"
echo "PoC: $W/poc-mac <infoHash> [folder] [seconds] [cacheMB]"
