#!/bin/bash
# Publishes nuvio-juntos (Watch Together rooms, servidor/juntos) and checks health.
#
# ONLY THIS SCRIPT PUBLISHES nuvio-juntos, and only from inside this tree: it
# checks that the wrangler.toml used is the one from servidor/juntos (name, Durable
# Object, without [assets]) before any deploy — same lesson from
# vidaa-publicar.sh and the VIDAA crash of 10/06.
#
# First time (or secret rotation): the SAME value in both Workers.
#   tools/juntos-publicar.sh --secret     # prompts for value, saves to both
# Afterwards:
#   tools/juntos-publicar.sh              # tests, publishes, and checks /health
#
# ORDER on first launch: migration-010 in D1 -> secret in both ->
# this script -> deploy nuvio-recomendacoes FROM master.
set -euo pipefail
cd "$(dirname "$0")/.."

DIR=servidor/juntos
URL="${NUVIO_JUNTOS_URL:-https://nuvio-juntos.henriquef29.workers.dev}"
W="$DIR/node_modules/.bin/wrangler"

grep -q '^name = "nuvio-juntos"$' "$DIR/wrangler.toml" || { echo "juntos-publicar.sh: $DIR/wrangler.toml is not for nuvio-juntos" >&2; exit 1; }
grep -q 'class_name = "Sala"' "$DIR/wrangler.toml" || { echo "juntos-publicar.sh: missing Durable Object Sala" >&2; exit 1; }
if grep -qE '^\[assets\]|^\[\[services\]\]' "$DIR/wrangler.toml"; then
  echo "juntos-publicar.sh: [assets] or [[services]] in nuvio-juntos — rejected" >&2; exit 1
fi
[ -x "$W" ] || (cd "$DIR" && npm install --no-audit --no-fund)

if [ "${1:-}" = "--secret" ] || [ "${1:-}" = "--segredo" ]; then
  read -r -s -p "SALA_SEGREDO (same in both Workers): " S; echo
  [ ${#S} -ge 32 ] || { echo "juntos-publicar.sh: use 32+ characters (e.g.: openssl rand -hex 32)" >&2; exit 1; }
  printf '%s' "$S" | "$W" secret put SALA_SEGREDO --config "$DIR/wrangler.toml"
  printf '%s' "$S" | "$W" secret put SALA_SEGREDO --config servidor/recomendacoes/wrangler.toml
  echo "secret saved in both; now run without --secret to publish."
  exit 0
fi

(cd "$DIR" && node --test teste-logica.mjs && node --test teste-juntos.mjs)
"$W" deploy --config "$DIR/wrangler.toml"

curl -fsS "$URL/saude" | grep -q '"ok":1' || { echo "juntos-publicar.sh: ERROR — $URL/saude does not respond" >&2; exit 1; }
echo "juntos-publicar.sh: nuvio-juntos live at $URL"
