# Functions to locate precompiled P2P engine. Used by: tools/android.sh,
# tools/arm.sh, tools/tpk.sh (and through them release scripts).
#
#   . tools/p2p-motor/pasta.sh
#   nv_p2p_resolver android     # sets NV_P2P_DIR (platform folder) or ""
#
# ROOT has one subfolder per platform, assembled by respective build-*.sh:
#   <root>/android/{lib/<abi>/*.a, include/}      tools/p2p-motor/build-android.sh
#   <root>/arm/{build-arm/..., nuvio-engine/include/}   build-arm.sh (LG webOS)
#   <root>/tpk/{libnuvio_engine.so, include/}     build-tpk.sh (Tizen 6+)
#   <root>/licenses/                              notices required by engine
#
# WHICH ROOT: $NUVIO_P2P_MOTOR; if unset, ~/.nuvio-p2p-motor or
# /Volumes/ExternalSSD/nuvio-p2p-motor, first existing. NUVIO_P2P_MOTOR=none
# compiles WITHOUT engine on purpose (dev build). No root found = no engine.
#
# FAIL LOUD: configured root missing a platform library = exit 2.
# Never publish package without engine by mistake.

nv_p2p_raiz() {
  NV_P2P_RAIZ=""
  if [ -n "${NUVIO_P2P_MOTOR:-}" ]; then
    [ "$NUVIO_P2P_MOTOR" = none ] || NV_P2P_RAIZ="$NUVIO_P2P_MOTOR"
    return 0
  fi
  local c
  for c in "$HOME/.nuvio-p2p-motor" /Volumes/ExternalSSD/nuvio-p2p-motor; do
    [ -d "$c" ] && { NV_P2P_RAIZ="$c"; return 0; }
  done
  return 0
}

nv_p2p_resolver() {  # $1 = android | arm | tpk
  nv_p2p_raiz
  NV_P2P_DIR=""
  [ -n "$NV_P2P_RAIZ" ] || { echo "  P2P engine: DISABLED (no directory; NUVIO_P2P_MOTOR=${NUVIO_P2P_MOTOR:-<empty>})" >&2; return 0; }
  local d="$NV_P2P_RAIZ/$1" f missing=""
  case "$1" in
    android) for f in arm64-v8a armeabi-v7a; do
               for l in libnuvio_engine.a libtorrent-rasterbar.a libssl.a libcrypto.a; do
                 [ -s "$d/lib/$f/$l" ] || missing="$missing lib/$f/$l"; done; done
             [ -s "$d/include/nuvio_engine/nuvio_engine.h" ] || missing="$missing include/nuvio_engine/nuvio_engine.h" ;;
    arm)     for l in build-arm/libnuvio_engine.a build-arm/_deps/nuvio_libtorrent-build/libtorrent-rasterbar.a \
                      nuvio-engine/include/nuvio_engine/nuvio_engine.h; do
               [ -s "$d/$l" ] || missing="$missing $l"; done ;;
    tpk)     for l in libnuvio_engine.so include/nuvio_engine/nuvio_engine.h; do
               [ -s "$d/$l" ] || missing="$missing $l"; done ;;
  esac
  if [ -n "$missing" ]; then
    echo "P2P engine: $d incomplete, missing:$missing" >&2
    echo "  run tools/p2p-motor/build-$1.sh, or NUVIO_P2P_MOTOR=none to compile WITHOUT engine intentionally" >&2
    exit 2
  fi
  NV_P2P_DIR="$d"
  echo "  P2P engine: ENABLED ($d)" >&2
}
