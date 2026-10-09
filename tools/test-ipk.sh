#!/bin/bash
# Verifies that .ipk does NOT contain user credentials — without requiring docker.
#
# Runs only the packaging portion of arm.sh (same staging, same exclusion list,
# same verification) using nuvio-proto already present in deploy/app.
# Normal development cycle requires compiling for ARM, and this avoids full 30s build.
#
# TWO PITFALLS this test prevents:
#   1. `ares-package deploy/app` includes entire directory, and art/ contains Trakt
#      tokens, addon URLs with debrid keys, TMDB key, and mdblist key.
#   2. .ipk is a Debian package: `tar tzf package.ipk` lists only
#      debian-binary / control.tar.gz / data.tar.gz without error.
#      A check written this way always passes even when secrets are inside.
#      Must unpack `ar` and inspect data.tar.gz.
set -e
cd "$(dirname "$0")/.."
ARES="${NUVIO_ARES_PACKAGE:-../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package}"
USER_FILES="trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt
            progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt home-pos.txt debrid.txt fanart.txt p2p.txt"
STAGING=$(mktemp -d)
trap 'rm -rf "$STAGING"' EXIT
cp -R deploy/app "$STAGING/app"
rm -rf "$STAGING/app/art/cache"
for f in $USER_FILES; do rm -f "$STAGING/app/art/$f"; done

OUTPUT_DIR=$(mktemp -d)
"$ARES" "$STAGING/app" -o "$OUTPUT_DIR" >/dev/null
IPK=$(ls -t "$OUTPUT_DIR"/*.ipk | head -1)

PKG_LIST=$(cd "$STAGING" && ar x "$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
[ -z "$PKG_LIST" ] && { echo "ABORTED: could not read package to verify"; exit 1; }

echo "test package: $(du -h "$IPK" | cut -f1)"
echo ".txt files inside art/:"
printf '%s\n' "$PKG_LIST" | grep -E "art/.*\.txt$" | sed 's|.*/art/|  |' | sort
LEAKED=""
for f in $USER_FILES; do
  printf '%s\n' "$PKG_LIST" | grep -q "art/$f$" && LEAKED="$LEAKED $f"
done
rm -rf "$OUTPUT_DIR"
if [ -n "$LEAKED" ]; then echo "FAILED: package contains credentials ->$LEAKED"; exit 1; fi
echo "OK: zero credentials in package"
