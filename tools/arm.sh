#!/bin/bash
# Compiles for ARM and installs on TV via DIRECT COPY. Full cycle (~30s).
#
#   bash tools/arm.sh            # compile, copy, verify, and launch
#   bash tools/arm.sh --build    # compile only
#   bash tools/arm.sh --ipk      # also generate .ipk (for distribution)
#   bash tools/arm.sh --ipk --build
#                                # generate .ipk and STOP: do not touch TV.
#                                # Mode for packaging a variant (webos3, high
#                                # cache) without disturbing installed app.
#   bash tools/arm.sh --high-cache [--ipk|--build]   (--alto-cache also accepted)
#                                # HIGH CACHE variant: texture budget locked
#                                # to 300 MB (NV_TEX_MB_FIXO), for TVs with
#                                # ample RAM. .ipk gets suffix -highcache and
#                                # title stamp indicates "high cache".
#                                # Standard build calculates budget dynamically
#                                # at launch from RAM.
#
# Does NOT use appInstallService: it reports success without replacing the binary.
#
# Image comes from tools/Dockerfile:
#   docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
#
# -ldl is required: video.c opens libluna-service2 via dlopen.
#
# Do NOT add -lcurl. rede.c opens libcurl via dlopen at runtime, trying so.5
# and then so.4. Linking curl introduces hard dependency on libcurl.so.4 in
# SDK, but TV only provides /usr/lib/libcurl.so.5 — binary fails to launch with
# "libcurl.so.4 => not found" with no visible error message on screen.
set -e
cd "$(dirname "$0")/.."

TV_IP="${NUVIO_TV_IP:-192.168.1.32}"
TV_PASS="${NUVIO_TV_PASS:-alpine}"
APP_ID="space.mkv.nuvio.native"
if [ -n "${NUVIO_ARES_PACKAGE:-}" ]; then
  ARES="$NUVIO_ARES_PACKAGE"
elif [ -x "../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package" ]; then
  ARES="../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package"
else
  ARES="$(command -v ares-package || echo ares-package)"
fi
CONTAINER_RUNTIME="${NUVIO_CONTAINER_RUNTIME:-docker}"
BUILD_PLATFORM="${NUVIO_BUILD_PLATFORM:-linux/arm64}"
SDK_IMAGE="${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}"
# DTS (PR #259): FFmpeg lives inside IMAGE. By default build requires
# conversion and fails if FFmpeg is missing; rebuild image in that case.
# NUVIO_DTS_FFMPEG=0 compiles without conversion; auto queries image and allows
# building without it when FFmpeg is unavailable.
DTS_ENABLED="${NUVIO_DTS_FFMPEG:-1}"
case "$DTS_ENABLED" in 0|1|auto) ;; *) echo 'NUVIO_DTS_FFMPEG must be 0, 1 or auto' >&2; exit 2;; esac
if [ "$DTS_ENABLED" = auto ]; then
  if "$CONTAINER_RUNTIME" run --rm --platform "$BUILD_PLATFORM" "$SDK_IMAGE" \
       sh -c 'test -f "${NUVIO_DTS_ROOT:-/opt/nuvio-dts}/include/libavcodec/avcodec.h"' 2>/dev/null; then
    DTS_ENABLED=1
  else
    DTS_ENABLED=0
    echo "==> WARNING: image $SDK_IMAGE lacks DTS FFmpeg; building WITHOUT DTS conversion"
    echo "    (docker build --platform linux/arm64 -t $SDK_IMAGE tools/  to include)"
  fi
fi

# --high-cache can come before or after --build/--ipk. Only alters a -D and names;
# code remains identical — variant does not require separate branch.
VARIANTE=""
if [ "$1" = "--alto-cache" ] || [ "$1" = "--high-cache" ]; then shift; VARIANTE="highcache"; fi
if [ "$2" = "--alto-cache" ] || [ "$2" = "--high-cache" ]; then set -- "$1"; VARIANTE="highcache"; fi
if [ "$3" = "--alto-cache" ] || [ "$3" = "--high-cache" ]; then set -- "$1" "$2"; VARIANTE="highcache"; fi
if [ "$VARIANTE" = "highcache" ]; then
  export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_TEX_MB_FIXO=300"
  echo "==> HIGH CACHE variant (300 MB textures)"
fi

echo "==> compiling for ARM"
# Server configuration enters via ENVIRONMENT VARIABLES and -Ds are assembled
# inside container. Passing them via `sh -c` command line would require nested quotes
# and fail silently: empty macro, binary builds and installs, login screen says
# "package assembled without server".
# Single cleanup function handles temporary files:
CLEANUP_FILES=""
cleanup() { [ -n "$CLEANUP_FILES" ] && rm -rf $CLEANUP_FILES; }
trap cleanup EXIT
ENVF=$(mktemp "${TMPDIR:-/tmp}/nuvio-arm-env.XXXXXXXX"); CLEANUP_FILES="$CLEANUP_FILES $ENVF"
tools/env.sh --env-file "$ENVF"

DTS_ENV=()
if [ -n "${NUVIO_DTS_ROOT:-}" ]; then DTS_ENV=(-e "NUVIO_DTS_ROOT=$NUVIO_DTS_ROOT"); fi
"$CONTAINER_RUNTIME" run --rm --platform "$BUILD_PLATFORM" --env-file "$ENVF" \
  -e NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-}" \
  -e NUVIO_ASS_LIBASS="${NUVIO_ASS_LIBASS:-1}" \
  -e NUVIO_DTS_FFMPEG="$DTS_ENABLED" \
  "${DTS_ENV[@]}" \
  -v "$PWD":/work:z "$SDK_IMAGE" sh -c '
  set -e
  SR=$NUVIO_SYSROOT
  ASS=$NUVIO_ASS_ROOT
  ASS_CFLAGS=""
  ASS_LIBS=""
  if [ "${NUVIO_ASS_LIBASS:-1}" = "1" ]; then
    [ -f "$ASS/include/ass/ass.h" ] || { echo "libass ARM missing in image; rebuild tools/Dockerfile or use NUVIO_ASS_LIBASS=0" >&2; exit 2; }
    ASS_CFLAGS="-DNV_ASS_LIBASS -I$ASS/include"
    ASS_LIBS="-L$ASS/lib -Wl,--start-group -lass -lharfbuzz -lfribidi -lfreetype -Wl,--end-group"
  fi
  DTS_CFLAGS=""
  DTS_LIBS=""
  if [ "${NUVIO_DTS_FFMPEG:-1}" = "1" ]; then
    DTS=${NUVIO_DTS_ROOT:-/opt/nuvio-dts}
    [ -f "$DTS/include/libavcodec/avcodec.h" ] || { echo "FFmpeg DTS missing; rebuild tools/Dockerfile or use NUVIO_DTS_FFMPEG=0" >&2; exit 2; }
    DTS_CFLAGS="-DNV_DTS_FFMPEG -I$DTS/include"
    DTS_LIBS="-L$DTS/lib -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lpthread -lm"
  fi
  arm-webos-linux-gnueabi-gcc src/*.c src/dts/*.c -o nuvio-proto.arm -O2 -DNV_WEBOS $NUVIO_EXTRA_CFLAGS $ASS_CFLAGS $DTS_CFLAGS \
    -DNV_SUPABASE_URL="\"$NV_SUPABASE_URL\"" \
    -DNV_SUPABASE_ANON_KEY="\"$NV_SUPABASE_ANON_KEY\"" \
    -DNV_TV_LOGIN_BASE="\"$NV_TV_LOGIN_BASE\"" \
    -DNV_TRAKT_CLIENT_ID="\"$NV_TRAKT_CLIENT_ID\"" \
    -DNV_TRAKT_CLIENT_SECRET="\"$NV_TRAKT_CLIENT_SECRET\"" \
    -DNV_SIMKL_CLIENT_ID="\"$NV_SIMKL_CLIENT_ID\"" \
    -DNV_SIMKL_APP="\"$NV_SIMKL_APP\"" \
    -DNV_TMDB_API_KEY="\"$NV_TMDB_API_KEY\"" \
    -DNV_SEEKR_API_KEY="\"$NV_SEEKR_API_KEY\"" \
    -DNV_VERSAO="\"$NV_VERSAO\"" \
    -I$SR/usr/include -I$SR/usr/include/SDL2 \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lz -lm $ASS_LIBS $DTS_LIBS \
    -Wl,--exclude-libs,ALL -Wl,--export-dynamic-symbol=pthread_create
  # src/dts/rt_shim.c: LG audio sink (KADP_OSA_CreateThread) binds to our pthread_create
  # Keep static FreeType private: older TV SDL_ttf must use its own ABI.
  mkdir -p deploy/app/lib
  find deploy/app/lib -maxdepth 1 -type f -name "dts-starfish-webos*.so" -delete
  if [ -d deploy/app/licenses/dts ]; then
    find deploy/app/licenses/dts -maxdepth 1 -type f \
      \( -name "COPYING.LGPLv2.1" -o -name "SOURCE.txt" \) -delete
  fi
  if [ "${NUVIO_DTS_FFMPEG:-1}" = "1" ]; then
    bash tools/build-dts-pipeline.sh --in-container --output deploy/app/lib
    mkdir -p deploy/app/licenses/dts
    cp "$DTS/COPYING.LGPLv2.1" "$DTS/SOURCE.txt" deploy/app/licenses/dts/
  fi'

# Verify configuration arrived in binary.
while IFS='=' read -r NAME VALOR; do
  [ -z "$NAME" ] && continue
  if [ -z "$VALOR" ]; then
    echo "    warning: $NAME empty in local.properties"
    continue
  fi
  if ! strings nuvio-proto.arm 2>/dev/null | grep -qF -- "$VALOR"; then
    echo "    ABORTED: $NAME not found in ARM binary"
    exit 1
  fi
done < "$ENVF"

cp nuvio-proto.arm deploy/app/nuvio-proto
rm -f ./*.ipk

# Package from clean copy, never deploy/app directly:
USER_FILES="trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt
            progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt
            listas.txt guia-fav.txt debrid.txt fanart.txt"

USER_GLOBS="stalker-p*.txt xtream-p*.txt listas-p*.txt trakt-p*.txt trakt-fluxo*.txt simkl*.txt conta-*.txt conta-*.txt.tmp jellyfin-p*.txt jellyfin-p*.txt.tmp emby-p*.txt emby-p*.txt.tmp plex-p*.txt plex-p*.txt.tmp"

USER_COLLECTIONS="collections.json catalogo-rede.bin catalogo-rede.bin.tmp"
USER_DIRS="collections"

if [ "$1" = "--ipk" ]; then
  echo "==> packaging (without credentials)"
  STAGING=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arm-pacote.XXXXXXXX"); CLEANUP_FILES="$CLEANUP_FILES $STAGING"
  cp -R deploy/app "$STAGING/app"
  rm -rf "$STAGING/app/art/cache"
  for f in $USER_FILES $USER_COLLECTIONS; do rm -f "$STAGING/app/art/$f"; done
  for g in $USER_GLOBS; do rm -f "$STAGING"/app/art/$g; done
  for d in $USER_DIRS; do rm -rf "$STAGING/app/art/$d"; done
  chmod 755 "$STAGING/app/nuvio-proto"

  "$ARES" "$STAGING/app" -o .
  IPK=$(ls -t ./*.ipk | head -1)
  if [ -n "$VARIANTE" ]; then
    NOVO="${IPK%.ipk}-$VARIANTE.ipk"
    mv -f "$IPK" "$NOVO"; IPK="$NOVO"
  fi

  # Verify finished package contents:
  PKG_LIST=$(cd "$STAGING" && ar x "$OLDPWD/$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
  if [ -z "$PKG_LIST" ]; then
    echo "    ABORTED: could not read package contents to verify; will not claim it is clean"
    rm -f "$IPK"
    exit 1
  fi
  LEAKED=""
  for f in $USER_FILES $USER_COLLECTIONS; do
    printf '%s\n' "$PKG_LIST" | grep -q "art/$f$" && LEAKED="$LEAKED $f"
  done
  for pre in stalker-p xtream-p listas-p trakt-p trakt-fluxo-p simkl-p; do
    printf '%s\n' "$PKG_LIST" | grep -qE "art/$pre[0-9]+\.txt$" && LEAKED="$LEAKED $pre*.txt"
  done
  printf '%s\n' "$PKG_LIST" | grep -qE "art/(trakt-fluxo|simkl)\.txt$" && LEAKED="$LEAKED trakt-fluxo.txt/simkl.txt"
  printf '%s\n' "$PKG_LIST" | grep -qE '(^|/)conta-[^/]*\.txt(\.tmp)?$' && LEAKED="$LEAKED conta-*.txt/conta-*.txt.tmp"
  printf '%s\n' "$PKG_LIST" | grep -qE '(^|/)(jellyfin|emby|plex)-p[^/]*\.txt(\.tmp)?$' && LEAKED="$LEAKED media-server profile tokens"
  for d in $USER_DIRS; do
    printf '%s\n' "$PKG_LIST" | grep -q "art/$d/" && LEAKED="$LEAKED $d/"
  done
  if [ -n "$LEAKED" ]; then
    echo "    ABORTED: package contains credentials ->$LEAKED"
    rm -f "$IPK"
    exit 1
  fi
  PKG_MODE=$(cd "$STAGING" && tar tvzf data.tar.gz 2>/dev/null | awk '/\/nuvio-proto$/ {print $1}')
  if [ "$PKG_MODE" != "-rwxr-xr-x" ]; then
    echo "    ABORTED: nuvio-proto in package has mode '${PKG_MODE:-missing}', expected -rwxr-xr-x (webOS will not execute)"
    rm -f "$IPK"
    exit 1
  fi
  for f in lib/dts-starfish-webos3.so lib/dts-starfish-webos4.so licenses/dts/COPYING.LGPLv2.1 licenses/dts/SOURCE.txt; do
    if printf '%s\n' "$PKG_LIST" | grep -qE "(^|/)$f$"; then has=1; else has=0; fi
    if [ "$has" != "$DTS_ENABLED" ]; then
      echo "    ABORTED: DTS=$DTS_ENABLED and package $([ $has = 1 ] && echo includes || echo does not include) $f"; rm -f "$IPK"; exit 1
    fi
  done
  echo "    DTS: $([ "$DTS_ENABLED" = 1 ] && echo 'conversion included (adapters + LGPL notice)' || echo 'without conversion')"
  echo "    $IPK ($(du -h "$IPK" | cut -f1)) — without art/{$(echo $USER_FILES $USER_GLOBS $USER_COLLECTIONS $USER_DIRS | tr ' ' ',')}"
fi

if [ "$1" = "--build" ] || [ "$2" = "--build" ]; then exit 0; fi

# Direct SSH deploy to rooted TV:
APPDIR=/media/developer/apps/usr/palm/applications/$APP_ID
SSH="sshpass -p $TV_PASS ssh -o StrictHostKeyChecking=no ${NUVIO_SSH_OPTS:-}"
SCP="sshpass -p $TV_PASS scp -o StrictHostKeyChecking=no -q ${NUVIO_SSH_OPTS:-}"

$SSH "root@$TV_IP" "mkdir -p $APPDIR"

DTS_PAYLOAD=()
if [ "$DTS_ENABLED" = 1 ]; then
  DTS_PAYLOAD=(lib/dts-starfish-webos3.so lib/dts-starfish-webos4.so
               licenses/dts/COPYING.LGPLv2.1 licenses/dts/SOURCE.txt)
  echo "==> deploying DTS adapters and license notices"
  $SSH "root@$TV_IP" "mkdir -p $APPDIR/lib $APPDIR/licenses/dts"
  for arquivo in "${DTS_PAYLOAD[@]}"; do
    $SCP "deploy/app/$arquivo" "root@$TV_IP:$APPDIR/$arquivo.novo"
    $SSH "root@$TV_IP" "mv -f $APPDIR/$arquivo.novo $APPDIR/$arquivo && chmod 644 $APPDIR/$arquivo"
  done
fi

echo "==> deploying binary to $TV_IP"
$SCP nuvio-proto.arm "root@$TV_IP:$APPDIR/nuvio-proto.novo"
$SSH "root@$TV_IP" "cd $APPDIR && mv -f nuvio-proto.novo nuvio-proto && chmod 755 nuvio-proto"

echo "==> stamping title with build"
STAMP=$(md5 -q nuvio-proto.arm 2>/dev/null || md5sum nuvio-proto.arm | cut -d' ' -f1)
STAMP=${STAMP:0:8}
[ -n "$VARIANTE" ] && STAMP="$STAMP high cache"
sed "s/\(\"title\": \"[^\"]*\)\"/\1 ($STAMP)\"/" deploy/app/appinfo.json > /tmp/appinfo.stamped.json
cp /tmp/appinfo.stamped.json deploy/app/appinfo.json.stamped

$SCP /tmp/appinfo.stamped.json "root@$TV_IP:$APPDIR/appinfo.json"

echo "==> synchronizing art"
if ! ( set -o pipefail
       tar czf - -C deploy/app --exclude 'art/cache' \
           --exclude 'appinfo.json.stamped' \
           art fonts icon.png icon-large.png icon-20.png icon-large-20.png splash.png \
         | $SSH "root@$TV_IP" "tar xzf - -C $APPDIR" ) 2>&1 \
     | grep -v 'unknown extended header keyword'; then
  :
fi
if ! $SSH "root@$TV_IP" "test -f $APPDIR/art/marcas/trakt.png && test -f $APPDIR/art/marcas/logo-novo-marca.png && test -f $APPDIR/art/marcas/abertura.jpg && test -f $APPDIR/art/marcas/login-fundo.jpg"; then
  echo "    FAILED: art did not reach TV"; exit 1
fi
rm -f deploy/app/appinfo.json.stamped
$SSH "root@$TV_IP" "rm -f $APPDIR/core; rm -f /tmp/nuvio-shot-req /tmp/nuvio-shot.bmp" || true

$SSH "root@$TV_IP" "mkdir -p $APPDIR/art/cache && chown -R 5152:5000 $APPDIR/art && chmod -R u+rwX $APPDIR/art"

echo "==> verifying"
LOCAL=$(md5 -q nuvio-proto.arm 2>/dev/null || md5sum nuvio-proto.arm | cut -d' ' -f1)
REMOTO=$($SSH "root@$TV_IP" "md5sum $APPDIR/nuvio-proto | cut -d' ' -f1" 2>/dev/null | tr -d '\r')
if [ "$LOCAL" != "$REMOTO" ]; then
  echo "    FAILED: local $LOCAL != TV $REMOTO"
  exit 1
fi
echo "    ok ($LOCAL)"
for arquivo in "${DTS_PAYLOAD[@]}"; do
  LOCAL=$(md5 -q "deploy/app/$arquivo" 2>/dev/null || md5sum "deploy/app/$arquivo" | cut -d' ' -f1)
  REMOTO=$($SSH "root@$TV_IP" "md5sum $APPDIR/$arquivo | cut -d' ' -f1" | tr -d '\r')
  if [ "$LOCAL" != "$REMOTO" ]; then
    echo "    FAILED: $arquivo local $LOCAL != TV $REMOTO"; exit 1
  fi
  echo "    ok ($arquivo)"
done

PID=$($SSH "root@$TV_IP" "pidof nuvio-proto" 2>/dev/null | tr -d "\r")
if [ -n "$PID" ]; then
  echo "==> stopping old process ($PID)"
  $SSH "root@$TV_IP" "kill $PID"
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 1
    NPID=$($SSH "root@$TV_IP" "pidof nuvio-proto" 2>/dev/null | tr -d "\r")
    [ -z "$NPID" ] && break
  done
  sleep 4
fi
echo "==> launching"
( sleep 2
  printf 'luna-send -n 1 -f luna://com.webos.applicationManager/launch '"'"'{"id":"%s"}'"'"'\n' "$APP_ID"
  sleep 3
  printf 'exit\n'
) | nc -w20 "$TV_IP" 23 | LC_ALL=C tr -d '\0' | grep -a returnValue
