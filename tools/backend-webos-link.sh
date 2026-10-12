#!/usr/bin/env bash
# Runs inside the existing webOS SDK container.
set -euo pipefail
compiler=arm-webos-linux-gnueabi-gcc
ass_flags=()
ass_libraries=()
if [ "${NUVIO_ASS_LIBASS:-1}" = 1 ]; then
  ass_flags=(-DNV_ASS_LIBASS "-I$NUVIO_ASS_ROOT/include")
  ass_libraries=("-L$NUVIO_ASS_ROOT/lib" -Wl,--start-group -lass -lharfbuzz -lfribidi -lfreetype -Wl,--end-group)
fi
definitions=(-DNV_WEBOS '-DNV_APP_ID="space.mkv.nuvio.rusttest"' '-DNV_APP_TITLE="Nuvio Rust Test"')
for name in NV_SUPABASE_URL NV_SUPABASE_ANON_KEY NV_TV_LOGIN_BASE NV_TRAKT_CLIENT_ID NV_TRAKT_CLIENT_SECRET NV_SIMKL_CLIENT_ID NV_SIMKL_APP NV_TMDB_API_KEY NV_SEEKR_API_KEY NV_VERSAO; do
  value=${!name:-}; value=${value//\\/\\\\}; value=${value//\"/\\\"}
  definitions+=("-D$name=\"$value\"")
done
"$compiler" src/*.c -o build/backend/webos/nuvio-rusttest.full -O2 -g \
  "${definitions[@]}" "${ass_flags[@]}" \
  -I"$NUVIO_SYSROOT/usr/include" -I"$NUVIO_SYSROOT/usr/include/SDL2" \
  build/backend/target/armv7-unknown-linux-gnueabi/release/libnuvio_backend.a \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lz -lm "${ass_libraries[@]}" \
  -Wl,--exclude-libs,ALL -Wl,--build-id=sha1
arm-webos-linux-gnueabi-objcopy --only-keep-debug build/backend/webos/nuvio-rusttest.full build/backend/webos/nuvio-rusttest.debug
cp build/backend/webos/nuvio-rusttest.full build/backend/webos/nuvio-rusttest
arm-webos-linux-gnueabi-strip --strip-unneeded build/backend/webos/nuvio-rusttest
arm-webos-linux-gnueabi-readelf -h -d -V build/backend/webos/nuvio-rusttest > build/backend/webos/elf-report.txt
chmod 755 build/backend/webos/nuvio-rusttest
