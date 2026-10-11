#!/bin/bash
# Exercise real data/log code with inherited production overrides.
set -euo pipefail
cd "$(dirname "$0")/.."
identity_scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rust-identity.XXXXXXXX")
trap 'rm -rf "$identity_scratch"' EXIT
mkdir "$identity_scratch/production"
printf 'production sentinel\n' > "$identity_scratch/production/settings.txt"
printf 'production account sentinel\n' > "$identity_scratch/production/production-only.txt"
read -r -a identity_cflags <<< "$(pkg-config --cflags sdl2)"
for profile in production rust; do
  identity_defines=(-DNV_LINUX_DESKTOP)
  if [ "$profile" = rust ]; then identity_defines+=(-DNV_RUST_CORE=1); fi
  cc -O1 -g -Isrc "${identity_cflags[@]}" "${identity_defines[@]}" \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    tests/rust_identity.c src/data.c src/log.c -o "$identity_scratch/$profile-probe"
done
env -u NUVIO_RUST_LOG NUVIO_DATA="$identity_scratch/production" NUVIO_LOG="$identity_scratch/production.log" \
  "$identity_scratch/rust-probe" "$identity_scratch/production" /tmp/space.mkv.nuvio.rusttest.log
env -u NUVIO_DATA NUVIO_DADOS="$identity_scratch/production" NUVIO_LOG="$identity_scratch/production.log" \
  NUVIO_RUST_LOG="$identity_scratch/rust.log" \
  "$identity_scratch/rust-probe" "$identity_scratch/production" "$identity_scratch/rust.log"
env -u NUVIO_LOG NUVIO_DATA="$identity_scratch/production" \
  "$identity_scratch/production-probe" "$identity_scratch/production" terminal
NUVIO_DATA="$identity_scratch/production" NUVIO_LOG="$identity_scratch/production.log" \
  "$identity_scratch/production-probe" "$identity_scratch/production" "$identity_scratch/production.log"
[ "$(cat "$identity_scratch/production/settings.txt")" = 'production sentinel' ]
echo 'Rust identity: PASS (package/media/diagnostics IDs, isolated settings, independent logs, production preserved)'
