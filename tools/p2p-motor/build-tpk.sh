#!/bin/bash
# P2P engine for .tpk (Samsung Tizen 6+): custom libnuvio_engine.so with
# libtorrent, OpenSSL, libstdc++, libgcc and libatomic EMBEDDED and only C API
# (nuvio_engine_*) exported.
#
#   bash tools/p2p-motor/build-tpk.sh [folder]    # default <root>/tpk (see pasta.sh)
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/tpk}"
COMMIT=02938d7
mkdir -p "$W"
[ -d "$W/nuvio-engine/.git" ] || git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
git -C "$W/nuvio-engine" checkout -q "$COMMIT"
docker run --rm --platform linux/arm64 -e NV_JOBS="${NV_JOBS:-2}" -v "$W":/w nuvio-webos-sdk sh -c '
  set -e
  command -v cmake >/dev/null 2>&1 && cmake --version >/dev/null 2>&1 || {
    apt-get update -qq >/dev/null && apt-get install -y -qq cmake git >/dev/null; }
  cat > /w/tpk.cmake <<T
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
  F="-fPIC -march=armv7-a -mfloat-abi=softfp -mfpu=neon -ffunction-sections -fdata-sections"
  CMAKE=/usr/bin/cmake
  $CMAKE -S /w/nuvio-engine -B /w/build-tpk -DCMAKE_TOOLCHAIN_FILE=/w/tpk.cmake \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON \
    -DNUVIO_ENGINE_BUILD_TESTS=OFF -DNUVIO_ENGINE_BUILD_SHARED=ON \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F -Wno-psabi" \
    -DCMAKE_SHARED_LINKER_FLAGS="-static-libstdc++ -static-libgcc -Wl,--gc-sections -Wl,-Bstatic -latomic -Wl,-Bdynamic" \
    >/w/cmake-tpk.log
  $CMAKE --build /w/build-tpk -j"${NV_JOBS:-3}" --target nuvio_engine >>/w/cmake-tpk.log
  S=$(ls /w/build-tpk/libnuvio_engine.so.*.*.* | head -1)
  cp "$S" /w/libnuvio_engine.so
  arm-webos-linux-gnueabi-strip --strip-unneeded /w/libnuvio_engine.so
  ls -l /w/libnuvio_engine.so
  arm-webos-linux-gnueabi-readelf -d /w/libnuvio_engine.so | grep -E "NEEDED|SONAME"
  echo "maximum glibc: $(arm-webos-linux-gnueabi-objdump -T /w/libnuvio_engine.so | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1)"
  echo "exported: $(arm-webos-linux-gnueabi-nm -D --defined-only /w/libnuvio_engine.so | grep -c " T nuvio_engine_")"
  arm-webos-linux-gnueabi-readelf -A /w/libnuvio_engine.so | grep -E "Tag_ABI_VFP_args|Tag_FP_arch|Tag_CPU_arch:"
'
rm -rf "$W/include"; cp -R "$W/nuvio-engine/include" "$W/include"
echo "ready in $W (libnuvio_engine.so + include/)"
