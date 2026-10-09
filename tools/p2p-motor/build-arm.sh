#!/bin/bash
# Compiles P2P engine (nuvio-engine + libtorrent 2.0.12 + static OpenSSL 3)
# for webOS ARMv7 using docker from tools/Dockerfile, and links PoC (poc.c)
# as static C++ binary to measure footprint.
#
#   bash tools/p2p-motor/build-arm.sh [working-directory]
#
# Working directory (defaults to <root>/arm, see pasta.sh) receives clone of
# nuvio-engine at pinned commit and build output; nothing commits to repository.
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/arm}"
COMMIT=02938d7
mkdir -p "$W"
if [ ! -d "$W/nuvio-engine/.git" ]; then
  git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
fi
git -C "$W/nuvio-engine" checkout -q "$COMMIT"
cp tools/p2p-motor/poc.c "$W/poc.c"
# Lower -j: with -j$(nproc), libtorrent cc1plus exceeds docker RAM (Killed).
docker run --rm --platform linux/arm64 -e NV_JOBS="${NV_JOBS:-2}" -v "$W":/w nuvio-webos-sdk sh -c '
  set -e
  command -v cmake >/dev/null 2>&1 && cmake --version >/dev/null 2>&1 || {
    apt-get update -qq >/dev/null && apt-get install -y -qq cmake git >/dev/null; }
  CMAKE=/usr/bin/cmake
  cat > /w/arm.cmake <<T
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7)
set(CMAKE_C_COMPILER arm-webos-linux-gnueabi-gcc)
set(CMAKE_CXX_COMPILER arm-webos-linux-gnueabi-g++)
set(CMAKE_SYSROOT \$ENV{NUVIO_SYSROOT})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(OPENSSL_USE_STATIC_LIBS TRUE)
T
  $CMAKE -S /w/nuvio-engine -B /w/build-arm -DCMAKE_TOOLCHAIN_FILE=/w/arm.cmake \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON \
    -DNUVIO_ENGINE_BUILD_TESTS=OFF \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -Wno-psabi" >/w/cmake-arm.log
  $CMAKE --build /w/build-arm -j"${NV_JOBS:-3}" >>/w/cmake-arm.log
  SR=$NUVIO_SYSROOT
  arm-webos-linux-gnueabi-g++ -Os -x c /w/poc.c -x none -I/w/nuvio-engine/include \
    /w/build-arm/libnuvio_engine.a /w/build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a \
    $SR/usr/lib/libssl.a $SR/usr/lib/libcrypto.a \
    -static-libstdc++ -static-libgcc -Wl,--gc-sections -lpthread -ldl -o /w/poc.arm
  cp /w/poc.arm /w/poc.arm.strip && arm-webos-linux-gnueabi-strip /w/poc.arm.strip
  ls -l /w/build-arm/libnuvio_engine.a /w/build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a /w/poc.arm /w/poc.arm.strip
  file /w/poc.arm.strip
  arm-webos-linux-gnueabi-readelf -d /w/poc.arm.strip | grep NEEDED
  arm-webos-linux-gnueabi-objdump -T /w/poc.arm.strip | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1
'
