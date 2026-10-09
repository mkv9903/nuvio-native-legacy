#!/bin/bash
# P2P engine (nuvio-engine + libtorrent 2.0.12 + OpenSSL 3.5.7, all static)
# for APK, via NDK from tools/android.sh, in arm64-v8a and armeabi-v7a.
#
#   bash tools/p2p-motor/build-android.sh [folder]    # default <root>/android (see pasta.sh)
#   bash tools/android.sh    # locates root automatically (NUVIO_P2P_MOTOR=none disables)
#
# OpenSSL comes from nuvio-engine's own script (scripts/build-android-openssl.sh,
# version and sha256 pinned there), tailored for 2 APK ABIs rather than 4.
# Requires cmake >= 3.24 in PATH.
set -eu
cd "$(dirname "$0")/../.."
. tools/p2p-motor/pasta.sh; nv_p2p_raiz
W="${1:-${NV_P2P_RAIZ:-/Volumes/ExternalSSD/nuvio-p2p-motor}/android}"
COMMIT=02938d7
SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
NDK="$SDK/ndk/27.2.12479018"
[ -d "$NDK" ] || { echo "build-android.sh: NDK missing at $NDK" >&2; exit 1; }
mkdir -p "$W"
[ -d "$W/nuvio-engine/.git" ] || git clone -q https://github.com/NuvioMedia/nuvio-engine "$W/nuvio-engine"
git -C "$W/nuvio-engine" checkout -q "$COMMIT"
E="$W/nuvio-engine"

sed -e 's/^abis=(.*/abis=(armeabi-v7a arm64-v8a)/' -e 's/^targets=(.*/targets=(android-arm android-arm64)/' \
  "$E/scripts/build-android-openssl.sh" > "$W/openssl-android.sh"
bash "$W/openssl-android.sh" "$NDK" "$W/openssl" >/dev/null

for abi in arm64-v8a armeabi-v7a; do
  B="$W/build-$abi"; OS="$W/openssl/install/$abi"
  # -fexperimental-library: engine uses std::jthread/stop_token, which in libc++
  # of NDK 27 is still experimental; nuvio-engine pins NDK 29.
  cmake -S "$E" -B "$B" -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-24 -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=MinSizeRel -DNUVIO_ENGINE_ENABLE_LIBTORRENT=ON -DNUVIO_ENGINE_BUILD_TESTS=OFF \
    -DOPENSSL_ROOT_DIR="$OS" -DOPENSSL_INCLUDE_DIR="$OS/include" -DOPENSSL_USE_STATIC_LIBS=TRUE \
    -DOPENSSL_SSL_LIBRARY="$OS/lib/libssl.a" -DOPENSSL_CRYPTO_LIBRARY="$OS/lib/libcrypto.a" \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -fexperimental-library" >"$W/cmake-$abi.log"
  cmake --build "$B" -j"${NV_JOBS:-8}" >>"$W/cmake-$abi.log"
  # Layout expected by CMakeLists.txt
  mkdir -p "$W/lib/$abi"
  cp "$B/libnuvio_engine.a" "$B/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a" \
     "$OS/lib/libssl.a" "$OS/lib/libcrypto.a" "$W/lib/$abi/"
done
rm -rf "$W/include"; cp -R "$E/include" "$W/include"
ls -l "$W"/lib/*/
echo "ready in $W (root is parent folder; tools/android.sh finds it automatically)"
