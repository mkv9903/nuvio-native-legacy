#!/bin/bash
# Reads local.properties (the SAME file as the web app) and prints the -Ds
# that compilation needs. No secrets enter code files: the value travels
# from the property directly to the compiler command line.
#
# The Supabase anon key is public per project — it is what RLS expects to receive,
# and the web app already publishes it in the bundle. What CANNOT enter the package
# is what used to live in art/trakt.txt and art/addons.txt: those are USER credentials.
#
#   eval "cc src/*.c $(tools/env.sh) ..."
set -e

REQUIRE_CORE=0
ALLOW_UNCONFIGURED=0
case "${1:-}" in
  "") ;;
  --require-core) REQUIRE_CORE=1 ;;
  --allow-unconfigured) ALLOW_UNCONFIGURED=1 ;;
  --env-file) ;;
  *)
    echo "env.sh: unknown option: $1" >&2
    exit 2
    ;;
esac

LOCAL_PROP="$(cd "$(dirname "$0")/.." && pwd)/local.properties"
if [ -f "$LOCAL_PROP" ]; then
  PROP="${NUVIO_PROPERTIES:-$LOCAL_PROP}"
else
  PROP="${NUVIO_PROPERTIES:-$(cd "$(dirname "$0")/../.." && pwd)/NuvioWeb-0.3.38-beta/local.properties}"
fi

val() {
  [ -f "$PROP" ] || return 0
  sed -n "s/^$1=//p" "$PROP" | head -1 | tr -d '\r'
}

URL=$(val NUVIO_SUPABASE_URL)
KEY=$(val NUVIO_SUPABASE_ANON_KEY)
TVB=$(val TV_LOGIN_WEB_BASE_URL)
TRK=$(val TRAKT_CLIENT_ID)
TRS=$(val TRAKT_CLIENT_SECRET)
SMK=$(val SIMKL_CLIENT_ID)
SMA=$(val SIMKL_APP_NAME)
TMD=$(val TMDB_API_KEY)
# Seekr key (timeline scrubber thumbnails) OF OWNER, embedded in package
# like TMDB. Empty = app prompts each user for key in Settings.
SKR=$(val SEEKR_API_KEY)
# App version comes from appinfo.json — SINGLE SOURCE OF TRUTH. It used to live
# in three places (appinfo.json, tizen-config.xml and a #define in ajustes.c) and
# the third stayed stuck on 1.0.44 for nine releases: Settings screen reported
# wrong version. Binary receives -DNV_VERSAO and arm.sh check requires finding
# value inside it, matching all other keys.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER=$(sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' "$ROOT/deploy/app/appinfo.json" | head -1)
if [ -z "$VER" ]; then
  echo "env.sh: \"version\" not found in deploy/app/appinfo.json" >&2; exit 2
fi

MISSING_CORE=()
[ -n "$URL" ] || MISSING_CORE+=(NUVIO_SUPABASE_URL)
[ -n "$KEY" ] || MISSING_CORE+=(NUVIO_SUPABASE_ANON_KEY)
[ -n "$TVB" ] || MISSING_CORE+=(TV_LOGIN_WEB_BASE_URL)
if [ "${#MISSING_CORE[@]}" -gt 0 ]; then
  if [ "$REQUIRE_CORE" -eq 1 ]; then
    # Production build cannot install and discover on login screen that it received empty macros.
    # List only names, never values or paths.
    echo "env.sh: mandatory configuration missing: ${MISSING_CORE[*]}" >&2
    exit 3
  elif [ "$ALLOW_UNCONFIGURED" -eq 0 ]; then
    # Permissive mode remains for explicit diagnostic harnesses.
    echo "env.sh: incomplete server configuration; use --require-core in release" >&2
  fi
fi

# --env-file: writes variables to file for `docker run --env-file`.
# Needed because ARM compilation runs INSIDE a container: passing -Ds on command
# line would require nested quotes within `sh -c`, which fails silently — compiler
# receives empty macro and app ships WITHOUT LOGIN, which happened on first TV deploy.
if [ "$1" = "--env-file" ]; then
  [ -n "$2" ] || { echo "env.sh --env-file requires path" >&2; exit 2; }
  {
    printf 'NV_SUPABASE_URL=%s\n' "$URL"
    printf 'NV_SUPABASE_ANON_KEY=%s\n' "$KEY"
    printf 'NV_TV_LOGIN_BASE=%s\n' "$TVB"
    printf 'NV_TRAKT_CLIENT_ID=%s\n' "$TRK"
    printf 'NV_TRAKT_CLIENT_SECRET=%s\n' "$TRS"
    printf 'NV_SIMKL_CLIENT_ID=%s\n' "$SMK"
    printf 'NV_SIMKL_APP=%s\n' "$SMA"
    printf 'NV_TMDB_API_KEY=%s\n' "$TMD"
    printf 'NV_SEEKR_API_KEY=%s\n' "$SKR"
    printf 'NV_VERSAO=%s\n' "$VER"
  } > "$2"
  chmod 600 "$2"
  exit 0
fi

printf -- '-DNV_SUPABASE_URL=\\"%s\\" -DNV_SUPABASE_ANON_KEY=\\"%s\\" -DNV_TV_LOGIN_BASE=\\"%s\\" -DNV_TRAKT_CLIENT_ID=\\"%s\\" -DNV_TRAKT_CLIENT_SECRET=\\"%s\\" -DNV_SIMKL_CLIENT_ID=\\"%s\\" -DNV_SIMKL_APP=\\"%s\\" -DNV_TMDB_API_KEY=\\"%s\\" -DNV_SEEKR_API_KEY=\\"%s\\" -DNV_VERSAO=\\"%s\\"' \
  "$URL" "$KEY" "$TVB" "$TRK" "$TRS" "$SMK" "$SMA" "$TMD" "$SKR" "$VER"
