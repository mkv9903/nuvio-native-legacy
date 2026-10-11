#!/bin/bash
# Compiles an optimized webOS release and installs on TV via DIRECT COPY.
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
#   bash tools/arm.sh --debug --ipk --build
#                                # unstripped optimized build, -debug IPK.
# Symbols for either mode are archived under build/webos/ by build ID.
#
# Does NOT use appInstallService: it reports success without replacing the binary.
#
# Image comes from tools/Dockerfile:
#   docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
#   podman build --platform linux/arm64 -t nuvio-webos-sdk tools/
# Docker is preferred when both engines are installed. Select explicitly with
# --podman/--docker or NUVIO_CONTAINER_RUNTIME (also accepts an executable path).
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
CONTAINER_RUNTIME="${NUVIO_CONTAINER_RUNTIME:-}"
SELECTED_RUNTIME=""
BUILD_PLATFORM="${NUVIO_BUILD_PLATFORM:-linux/arm64}"
SDK_IMAGE="${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}"
BUILD_ONLY=0
MAKE_IPK=0
BUILD_MODE=release
SELECTED_MODE=""
VARIANTE=""
for option in "$@"; do
  case "$option" in
    --build) BUILD_ONLY=1 ;;
    --ipk) MAKE_IPK=1 ;;
    --high-cache|--alto-cache) VARIANTE=highcache ;;
    --podman|--docker)
      runtime=${option#--}
      if [ -n "$SELECTED_RUNTIME" ] && [ "$SELECTED_RUNTIME" != "$runtime" ]; then
        echo 'arm.sh: choose either --podman or --docker' >&2; exit 2
      fi
      CONTAINER_RUNTIME=$runtime; SELECTED_RUNTIME=$runtime ;;
    --release|--debug)
      mode=${option#--}
      if [ -n "$SELECTED_MODE" ] && [ "$SELECTED_MODE" != "$mode" ]; then
        echo 'arm.sh: choose either --release or --debug' >&2; exit 2
      fi
      BUILD_MODE=$mode; SELECTED_MODE=$mode ;;
    --help|-h)
      echo 'Usage: bash tools/arm.sh [--podman|--docker] [--release|--debug] [--ipk] [--build] [--high-cache]'
      echo 'Default: optimized, stripped release with separate symbols in build/webos/.'
      echo 'Use --build to stop before TV deployment; --ipk also creates a package.'
      echo 'Runtime: explicit flag, then NUVIO_CONTAINER_RUNTIME, then Docker/Podman detection.'
      exit 0 ;;
    *) echo "arm.sh: unknown option: $option" >&2; exit 2 ;;
  esac
done
if [ -z "$CONTAINER_RUNTIME" ]; then
  if command -v docker > /dev/null 2>&1; then
    CONTAINER_RUNTIME=docker
  elif command -v podman > /dev/null 2>&1; then
    CONTAINER_RUNTIME=podman
  else
    echo 'arm.sh: install Docker or Podman, or set NUVIO_CONTAINER_RUNTIME to an executable' >&2
    exit 2
  fi
fi
if ! command -v "$CONTAINER_RUNTIME" > /dev/null 2>&1; then
  echo "arm.sh: container runtime not found: $CONTAINER_RUNTIME" >&2
  exit 2
fi
if [ "$VARIANTE" = "highcache" ]; then
  export NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-} -DNV_TEX_MB_FIXO=300"
  echo "==> HIGH CACHE variant (300 MB textures)"
fi

BUILD_COMMIT=$(git rev-parse HEAD)
BUILD_DIRTY=0
BUILD_LABEL=${BUILD_COMMIT:0:12}
if [ -n "$(git status --porcelain)" ]; then
  BUILD_DIRTY=1; BUILD_LABEL="$BUILD_LABEL-dirty"
fi

echo "==> compiling for ARM ($BUILD_MODE)"
echo "    container runtime: $CONTAINER_RUNTIME"
# Server configuration enters via ENVIRONMENT VARIABLES and -Ds are assembled
# inside container. Passing them via `sh -c` command line would require nested quotes
# and fail silently: empty macro, binary builds and installs, login screen says
# "package assembled without server".
# Single cleanup function handles temporary files:
CLEANUP_FILES=()
cleanup() { rm -rf -- "${CLEANUP_FILES[@]}"; }
trap cleanup EXIT
ENVF=$(mktemp "${TMPDIR:-/tmp}/nuvio-arm-env.XXXXXXXX"); CLEANUP_FILES+=("$ENVF")
tools/env.sh --env-file "$ENVF"

# Container variables must expand inside the SDK, not on the host.
# shellcheck disable=SC2016
"$CONTAINER_RUNTIME" run --rm --platform "$BUILD_PLATFORM" --env-file "$ENVF" \
  -e NUVIO_EXTRA_CFLAGS="${NUVIO_EXTRA_CFLAGS:-}" \
  -e NUVIO_ASS_LIBASS="${NUVIO_ASS_LIBASS:-1}" \
  -e NUVIO_BUILD_MODE="$BUILD_MODE" \
  -e NUVIO_BUILD_COMMIT="$BUILD_COMMIT" \
  -e NUVIO_BUILD_DIRTY="$BUILD_DIRTY" \
  -e NUVIO_BUILD_LABEL="$BUILD_LABEL" \
  -v "$PWD":/work:z "$SDK_IMAGE" sh -c '
  set -e
  SR=$NUVIO_SYSROOT
  ASS=$NUVIO_ASS_ROOT
  ASS_CFLAGS=""
  ASS_LIBS=""
  if [ "${NUVIO_ASS_LIBASS:-1}" = "1" ]; then
    [ -f "$ASS/include/ass/ass.h" ] || { echo "libass ARM missing in image; rebuild tools/Dockerfile or use NUVIO_ASS_LIBASS=0" >&2; exit 2; }
    if [ ! -f "$ASS/.nuvio-build-type" ] || [ "$(cat "$ASS/.nuvio-build-type")" != debugoptimized ]; then
      echo "ARM subtitle libraries need rebuilding: rebuild tools/Dockerfile with Docker or Podman (see README.md)" >&2
      exit 2
    fi
    ASS_CFLAGS="-DNV_ASS_LIBASS -I$ASS/include"
    ASS_LIBS="-L$ASS/lib -Wl,--start-group -lass -lharfbuzz -lfribidi -lfreetype -Wl,--end-group"
  fi
  arm-webos-linux-gnueabi-gcc src/*.c -o nuvio-proto.arm -O2 -g -DNV_WEBOS $NUVIO_EXTRA_CFLAGS $ASS_CFLAGS \
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
    -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lz -lm $ASS_LIBS \
    -Wl,--exclude-libs,ALL -Wl,--build-id=sha1
  # Keep static FreeType private: older TV SDL_ttf must use its own ABI.
  mkdir -p build/webos
  NUVIO_OBJCOPY=arm-webos-linux-gnueabi-objcopy \
  NUVIO_STRIP=arm-webos-linux-gnueabi-strip \
  NUVIO_READELF=arm-webos-linux-gnueabi-readelf \
  NUVIO_ARTIFACT_PLATFORM=webos NUVIO_ARTIFACT_VERSION="$NV_VERSAO" \
    bash tools/split-symbols.sh nuvio-proto.arm "build/webos/$NUVIO_BUILD_LABEL" "$NUVIO_BUILD_MODE" \
    > "build/webos/latest-$NUVIO_BUILD_MODE.txt"
  # Remove generated payloads left by builds of the retired converter.
  rm -f deploy/app/lib/dts-starfish-webos*.so
  rm -rf deploy/app/licenses/dts
'
echo "    symbols and build hashes: $(cat "build/webos/latest-$BUILD_MODE.txt")"

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

# Package from clean copy, never deploy/app directly:
USER_FILES="trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt
            progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt
            listas.txt guia-fav.txt debrid.txt fanart.txt"

USER_COLLECTIONS="collections.json catalogo-rede.bin catalogo-rede.bin.tmp"
USER_DIRS="collections"

# The same allowlisted payload is used by IPKs and direct SSH deployments.
STAGING=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arm-pacote.XXXXXXXX"); CLEANUP_FILES+=("$STAGING")
bash tools/stage-webos.sh deploy/app "$STAGING/app"

if [ "$MAKE_IPK" = 1 ]; then
  echo "==> packaging (without credentials)"
  mkdir "$STAGING/packages"
  "$ARES" "$STAGING/app" -o "$STAGING/packages"
  PACKAGES=("$STAGING/packages/"*.ipk)
  if [ "${#PACKAGES[@]}" -ne 1 ] || [ ! -f "${PACKAGES[0]}" ]; then
    echo '    ABORTED: expected exactly one generated IPK' >&2; exit 1
  fi
  IPK=${PACKAGES[0]}
  PACKAGE_NAME=$(basename "${IPK%.ipk}")
  [ -n "$VARIANTE" ] && PACKAGE_NAME="$PACKAGE_NAME-$VARIANTE"
  [ "$BUILD_MODE" = debug ] && PACKAGE_NAME="$PACKAGE_NAME-debug"
  PACKAGE_NAME="$PACKAGE_NAME.ipk"

  # Verify finished package contents:
  PKG_LIST=$(cd "$STAGING" && ar x "$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
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
    printf '%s\n' "$PKG_LIST" | grep -qE "art/${pre}[0-9]+\.txt$" && LEAKED="$LEAKED $pre*.txt"
  done
  printf '%s\n' "$PKG_LIST" | grep -qE "art/(trakt-fluxo|simkl)\.txt$" && LEAKED="$LEAKED trakt-fluxo.txt/simkl.txt"
  printf '%s\n' "$PKG_LIST" | grep -qE '(^|/)conta-[^/]*\.txt(\.tmp)?$' && LEAKED="$LEAKED conta-*.txt/conta-*.txt.tmp"
  printf '%s\n' "$PKG_LIST" | grep -qE '(^|/)(jellyfin|emby|plex)-p[^/]*\.txt(\.tmp)?$' && LEAKED="$LEAKED media-server profile tokens"
  for d in $USER_DIRS; do
    printf '%s\n' "$PKG_LIST" | grep -q "art/$d/" && LEAKED="$LEAKED $d/"
  done
  printf '%s\n' "$PKG_LIST" | grep -qE 'art/(cinematic|editorial|poster|logo|ep|elenco|prov)/|art/[0-9][0-9]\.jpg$|art/(catalogo|ids|elenco|episodios|extra|LEIA-ME-addons)\.txt$' && LEAKED="$LEAKED sample catalog/artwork"
  printf '%s\n' "$PKG_LIST" | grep -qE '(^|/)(build|symbols)/|\.debug$|(^|/)build\.json$|\.(full|unstripped)$' && LEAKED="$LEAKED developer symbol artifacts"
  if [ -n "$LEAKED" ]; then
    echo "    ABORTED: package contains private or sample data ->$LEAKED"
    rm -f "$IPK"
    exit 1
  fi
  PKG_MODE=$(cd "$STAGING" && tar tvzf data.tar.gz 2>/dev/null | awk '/\/nuvio-proto$/ {print $1}')
  if [ "$PKG_MODE" != "-rwxr-xr-x" ]; then
    echo "    ABORTED: nuvio-proto in package has mode '${PKG_MODE:-missing}', expected -rwxr-xr-x (webOS will not execute)"
    rm -f "$IPK"
    exit 1
  fi
  cp "$IPK" "$PACKAGE_NAME"
  echo "    $PACKAGE_NAME ($(du -h "$PACKAGE_NAME" | cut -f1)) — without private data, sample catalogs/artwork, or symbol files"
fi

if [ "$BUILD_ONLY" = 1 ]; then exit 0; fi

# Direct SSH deploy to rooted TV:
APPDIR=/media/developer/apps/usr/palm/applications/$APP_ID
SSH="sshpass -p $TV_PASS ssh -o StrictHostKeyChecking=no ${NUVIO_SSH_OPTS:-}"
SCP="sshpass -p $TV_PASS scp -o StrictHostKeyChecking=no -q ${NUVIO_SSH_OPTS:-}"

$SSH "root@$TV_IP" "mkdir -p $APPDIR"

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
       tar czf - -C "$STAGING/app" \
           art fonts icon.png icon-large.png icon-20.png icon-large-20.png splash.png \
         | $SSH "root@$TV_IP" "tar xzf - -C $APPDIR" ) 2>&1 \
     | grep -v 'unknown extended header keyword'; then
  :
fi
if ! $SSH "root@$TV_IP" "test -f $APPDIR/art/marcas/trakt.png && test -f $APPDIR/art/marcas/logo-novo-marca.png && test -f $APPDIR/art/marcas/abertura.jpg && test -f $APPDIR/art/marcas/login-fundo.jpg"; then
  echo "    FAILED: art did not reach TV"; exit 1
fi
rm -f deploy/app/appinfo.json.stamped
# Drop sample assets left by an older deployment, keeping user data/cache.
$SSH "root@$TV_IP" "rm -rf $APPDIR/art/cinematic $APPDIR/art/editorial $APPDIR/art/poster $APPDIR/art/logo $APPDIR/art/ep $APPDIR/art/elenco $APPDIR/art/prov; rm -f $APPDIR/art/[0-9][0-9].jpg $APPDIR/art/catalogo.txt $APPDIR/art/ids.txt $APPDIR/art/elenco.txt $APPDIR/art/episodios.txt $APPDIR/art/extra.txt $APPDIR/art/LEIA-ME-addons.txt"
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
