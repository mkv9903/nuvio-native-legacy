#!/bin/bash
# Exercise packaging modes/order without contacting a TV or using real keys.
set -euo pipefail
cd "$(dirname "$0")/.."
scratch=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-arm-profile-test.XXXXXXXX")
trap 'rm -rf "$scratch"' EXIT
mkdir -p "$scratch/tools" "$scratch/deploy/app/art" "$scratch/deploy/app/fonts" "$scratch/bin"
cp tools/arm.sh tools/env.sh tools/stage-webos.sh tools/split-symbols.sh "$scratch/tools/"
printf '{"id":"space.mkv.nuvio.native",\n"version":"1.2.3"}\n' > "$scratch/deploy/app/appinfo.json"
for file in nuvio-proto icon.png icon-large.png icon-20.png icon-large-20.png splash.png; do
  printf 'test asset\n' > "$scratch/deploy/app/$file"
done
printf 'font\n' > "$scratch/deploy/app/fonts/font.ttf"
for dir in badges icones icones-app marcas selos; do mkdir -p "$scratch/deploy/app/art/$dir"; done
printf 'suggestions\n' > "$scratch/deploy/app/art/addons-recomendados.txt"
cat > "$scratch/bin/git" <<'SH'
#!/bin/bash
case "$1" in rev-parse) printf '0123456789abcdef\n';; status) :;; *) exit 2;; esac
SH
cat > "$scratch/bin/container" <<'SH'
#!/bin/bash
set -euo pipefail
printf '%s\n' "${0##*/}" > "$NUVIO_TEST_RUNTIME_LOG"
while [ "$#" -gt 0 ]; do
  case "$1" in
    --env-file) set -a; source "$2"; set +a; shift 2 ;;
    -e) export "$2"; shift 2 ;;
    -v) cd "${2%:/work:z}"; shift 2 ;;
    *) shift ;;
  esac
done
printf '#include <stdio.h>\nint main(void) { puts("%s"); return 0; }\n' "$NV_VERSAO" > sample.c
cc -O2 -g -Wl,--build-id=sha1 sample.c -o nuvio-proto.arm
mkdir -p build/webos
NUVIO_ARTIFACT_PLATFORM=webos NUVIO_ARTIFACT_VERSION="$NV_VERSAO" \
  bash tools/split-symbols.sh nuvio-proto.arm "build/webos/$NUVIO_BUILD_LABEL" "$NUVIO_BUILD_MODE" \
  > "build/webos/latest-$NUVIO_BUILD_MODE.txt"
SH
cat > "$scratch/bin/ares" <<'SH'
#!/bin/bash
set -euo pipefail
source_app=$1; output=$3
mkdir -p "$output/arwork/usr/palm/applications/space.mkv.nuvio.native"
cp -R "$source_app/." "$output/arwork/usr/palm/applications/space.mkv.nuvio.native/"
tar czf "$output/data.tar.gz" -C "$output/arwork" usr
printf '2.0\n' > "$output/debian-binary"
ar r "$output/space.mkv.nuvio.native_1.2.3_arm.ipk" "$output/debian-binary" "$output/data.tar.gz" > /dev/null 2>&1
SH
cat > "$scratch/bin/sshpass" <<'SH'
#!/bin/bash
echo 'FAIL: attempted TV deployment during --build' >&2
exit 99
SH
chmod +x "$scratch/bin/"* "$scratch/tools/env.sh"
export PATH="$scratch/bin:$PATH" NUVIO_CONTAINER_RUNTIME="$scratch/bin/container"
export NUVIO_ARES_PACKAGE="$scratch/bin/ares" NUVIO_PROPERTIES="$scratch/no-keys"
export NUVIO_TEST_RUNTIME_LOG="$scratch/runtime.log"
unset NUVIO_EXTRA_CFLAGS
bash "$scratch/tools/arm.sh" --help > /dev/null
for options in '--unknown' '--release --debug' '--podman --docker' '--docker --podman'; do
  read -r -a args <<< "$options"
  if bash "$scratch/tools/arm.sh" "${args[@]}" > /dev/null 2>&1; then
    echo "FAIL: accepted $options" >&2; exit 1
  fi
done
run_build() { bash "$scratch/tools/arm.sh" "$@" > "$scratch/build.log" 2>&1 || { cat "$scratch/build.log"; return 1; }; }
# Default release, --build last. Retain earlier packages rather than deleting all.
run_build --ipk --build
release="$scratch/space.mkv.nuvio.native_1.2.3_arm.ipk"
checksum=$(sha256sum "$release")
readelf -S "$scratch/nuvio-proto.arm" > "$scratch/sections"
if grep -q '\.debug_info' "$scratch/sections"; then echo 'FAIL: default release not stripped' >&2; exit 1; fi
# --build precedes --debug and --ipk; ensure release is untouched.
run_build --build --debug --ipk
[ "$checksum" = "$(sha256sum "$release")" ]
[ -f "$scratch/space.mkv.nuvio.native_1.2.3_arm-debug.ipk" ]
readelf -S "$scratch/nuvio-proto.arm" | grep -q '\.debug_info'
# All four flags, including build-only in the middle.
run_build --debug --build --high-cache --ipk
[ -f "$scratch/space.mkv.nuvio.native_1.2.3_arm-highcache-debug.ipk" ]

# Exercise engine selection in a PATH without the host's Docker/Podman. The
# runtime fixture still compiles and packages an executable through arm.sh.
mkdir "$scratch/host-tools"
for tool in bash dirname mktemp rm cp mkdir awk sed head tr cat ar tar gzip basename du cut strings grep chmod sha256sum readelf objcopy strip python3 cc as ld; do
  ln -s "$(command -v "$tool")" "$scratch/host-tools/$tool"
done
ln -s "$scratch/bin/git" "$scratch/host-tools/git"
ln -s "$scratch/bin/container" "$scratch/host-tools/docker"
ln -s "$scratch/bin/container" "$scratch/host-tools/podman"
assert_runtime() {
  [ "$(cat "$scratch/runtime.log")" = "$1" ] || {
    echo "FAIL: expected $1 runtime" >&2; exit 1
  }
}
NUVIO_CONTAINER_RUNTIME='' PATH="$scratch/host-tools" run_build --build
assert_runtime docker
PATH="$scratch/host-tools" run_build --podman --release --ipk --build
assert_runtime podman
NUVIO_CONTAINER_RUNTIME=podman PATH="$scratch/host-tools" run_build --docker --build
assert_runtime docker
NUVIO_CONTAINER_RUNTIME=podman PATH="$scratch/host-tools" run_build --build
assert_runtime podman
rm "$scratch/host-tools/docker"
NUVIO_CONTAINER_RUNTIME='' PATH="$scratch/host-tools" run_build --build
assert_runtime podman
rm "$scratch/host-tools/podman"
# Help needs no runtime, and unavailable engines fail before compilation.
PATH="$scratch/host-tools" bash "$scratch/tools/arm.sh" --help > /dev/null
rm "$scratch/runtime.log"
if NUVIO_CONTAINER_RUNTIME='' PATH="$scratch/host-tools" bash "$scratch/tools/arm.sh" --build > "$scratch/build.log" 2>&1; then
  echo 'FAIL: accepted missing container engines' >&2; exit 1
fi
grep -q 'install Docker or Podman' "$scratch/build.log"
if PATH="$scratch/host-tools" bash "$scratch/tools/arm.sh" --podman --build > "$scratch/build.log" 2>&1; then
  echo 'FAIL: accepted unavailable explicit Podman' >&2; exit 1
fi
grep -q 'container runtime not found: podman' "$scratch/build.log"
[ ! -e "$scratch/runtime.log" ]
echo 'ARM profiles: PASS (default release, debug/high-cache names, flag order, release preserved, no TV contact)'
echo 'ARM runtimes: PASS (Docker priority, Podman fallback, flag/env/path selection, missing engines, help without runtime)'
