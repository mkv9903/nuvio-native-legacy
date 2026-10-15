# Nuvio Native — personal LG webOS fork

On `codex/rust-backend`, complete backend capabilities are being rewritten in
idiomatic Rust while preserving the cleaned C UI's design. C models and callers
adapt to Rust-owned state and results. See the
[active backend rewrite plan](docs/backend-rewrite/README.md).

This fork retains the LG webOS app and a Linux desktop preview for testing its UI.
The UI uses C, SDL2 and GLES2, with account capabilities owned by Rust.
LG playback uses the TV's media pipeline;
Linux playback is stubbed.

Based on [Nuvio Native Legacy](https://github.com/iqui27/nuvio-native-legacy).
Upstream attribution and third-party licenses are retained.

This branch builds **Nuvio Rust Test**, package ID `space.mkv.nuvio.rusttest`,
alongside the production app. Rewritten areas start with fresh versioned Rust
JSON; there is no import of C data or discarded Rust experiments. Account data
is stored under the test app's `rust-backend-v1/account.json`. Signing into the
same Nuvio account retrieves its remote data.

Android, Samsung/Tizen, WebAssembly/VIDAA and macOS builds have been removed,
along with the previously deleted social, P2P, Discord and personal media-server
features. Update checks use releases from
[this fork](https://github.com/mkv9903/nuvio-native-legacy/releases). The announcement feed, donation
UI and log/report uploads have been removed. Logs, crash reports and diagnostics
stay on the device. Settings help and previews, and first-use explanations remain
available; the separate User guide has been removed.
New Rust code and tests use English. Complete C UI components are translated
when connected to the Rust backend, while preserving their design.

## Linux UI preview

On Ubuntu, install the development dependencies:

```bash
sudo apt install build-essential pkg-config libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libgles2-mesa-dev libegl1-mesa-dev zlib1g-dev
```

On Fedora, install the development dependencies by their library names:

```bash
sudo dnf install gcc pkgconf-pkg-config \
  'pkgconfig(sdl2)' 'pkgconfig(SDL2_image)' 'pkgconfig(SDL2_ttf)' \
  'pkgconfig(glesv2)' 'pkgconfig(egl)' 'pkgconfig(zlib)'
```

Build and run locally with the normal login flow:

```bash
rustup toolchain install 1.85.1 --profile minimal
bash tools/linux.sh
```

To browse the UI without an account, use `bash tools/linux.sh --preview`.
The script builds for the local compiler's architecture, including amd64/x86-64
and ARM64. Build on the machine where you intend to run the executable.

Build without launching:

```bash
bash tools/linux.sh --build
```

Account tests: `bash tools/backend.sh --test`. The real C login/profile screens
can be checked with the Rust UI driver: `bash tools/backend-ui-tests.sh`
(requires Xvfb and ImageMagick in addition to the native UI dependencies).
Both use fake accounts; no credentials are needed for automated tests.

The isolated TV build is `bash tools/backend-webos.sh --ipk`. It uses the
existing SDK image and Rust target `armv7-unknown-linux-gnueabi`. It never
installs or launches the app; packages are written to `build/backend/webos`.
Verify an existing package with `bash tools/test-ipk.sh path/to/package.ipk`.
For the first account milestone, follow the
[Linux and TV check steps](docs/backend-rewrite/first-tv-check.md).
For the current integrated catalog build, follow the
[catalog and profile test steps](docs/backend-rewrite/catalog-runtime-testing.md).

The executable is `build/linux/nuvio-rusttest`. Local data defaults to
`${XDG_DATA_HOME:-$HOME/.local/share}/space.mkv.nuvio.rusttest`; preview mode uses
`${XDG_DATA_HOME:-$HOME/.local/share}/nuvio-rust-preview/space.mkv.nuvio.rusttest`.
Set `NUVIO_DATA` to override the parent data directory.
Navigate with arrows, Enter,
Escape or the mouse.

## LG webOS build

Build the SDK image and compile/package without deploying to a TV:

```bash
docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
bash tools/arm.sh --build
bash tools/backend-webos.sh --ipk
```

Podman is also supported, including rootless builds on Fedora:

```bash
podman build --platform linux/arm64 -t nuvio-webos-sdk tools/
NUVIO_CONTAINER_RUNTIME=podman bash tools/backend-webos.sh --ipk
```

The backend build uses Docker by default. Set `NUVIO_CONTAINER_RUNTIME` to
`podman` or another compatible executable path. Build the SDK image with the
same engine you use to run it; Docker and Podman normally have separate image
stores. On a host using SELinux, allow the container to access the project mount.

`linux/arm64` is the SDK container architecture; the resulting TV executable is
32-bit ARMv7. On an amd64/x86-64 build machine, running this ARM64 SDK requires
host AArch64 emulation through QEMU/binfmt. Setting `--platform` alone does not
install that emulation. `NUVIO_BUILD_PLATFORM` and `NUVIO_SDK_IMAGE` can override
the platform and image when using a different compatible SDK.

On Fedora amd64, install the AArch64 emulator and register its binfmt handler
before building the SDK image:

```bash
sudo dnf install podman qemu-user-static-aarch64
sudo systemctl restart systemd-binfmt
```

Packaging requires `ares-package`. Both LG and Linux builds read server/API
configuration from `local.properties` in the repository root. Create it from
[local.properties.example](local.properties.example) if you do not already have
one; the example documents all nine supported properties. Use plain `NAME=value`
lines without quotes or `export`. The real file is ignored by Git.

The Supabase URL/client key configure account login and sync; the TV login page
URL enables authorization from a phone. Trakt, Simkl, TMDB and Seekr values are
optional integration settings. Values are compiled into the executable, so
rebuild after changing them. To read a different configuration file, set
`NUVIO_PROPERTIES=/absolute/path/local.properties` when running either build
script. Individual property values exported in the shell are not read by
`tools/env.sh`. App version comes from `deploy/app/appinfo.json`.

The SDK includes libass for styled subtitles.
See the [installation guide](INSTALL.md).

Backend webOS builds use optimized Rust and C code with debug sections removed
from the shipped executable. Logging, local diagnostics and crash capture stay
enabled. The unstripped executable, separate symbols and ELF compatibility report
are saved alongside the stripped executable under `build/backend/webos`.
Copy that directory before replacing a build if its symbols may be needed to
diagnose a TV crash. Symbols are excluded from the IPK and Git. Linux builds
continue to use development symbols.

LG playback uses the TV's native player. The app does not bundle FFmpeg or
convert DTS audio, and there is no experimental Dolby Vision MKV pipeline.
Audio and video format support depends on the TV. The Dolby Vision and Dolby
Atmos source preferences affect source selection, not decoding.

On this branch, `tools/arm.sh` delegates to the isolated backend build. It never
installs or launches the app on a TV. The supported options are `--build` and
`--ipk`; older deployment, debug and high-cache options do not apply here.

TV packages and direct deployments include only runtime UI resources. Sample
catalogs, posters, title logos, cast/episode photos, and custom collection banners
stay in the repository for Linux previews and screenshot tests. On TV, Home
uses the signed-in account's catalog and local cache; settings and guides use
available catalog images or neutral placeholders. Fonts, icons, format/rating
badges, app artwork, guide content, and curated TV-addon suggestions are retained.

LG data defaults to `~/.space.mkv.nuvio.rusttest`, with an app-specific fallback
under `/media/developer/temp/`. Local logs use
`/tmp/space.mkv.nuvio.rusttest.log`. You will need to sign in on the new installation.
The updater uses this fork's GitHub releases and will offer a compatible LG
package when a newer release is published with an IPK asset. Until then,
manual update checks may report that a release could not be retrieved.

## Checks and maintained inputs

Tests live in `tests/`. `bash tools/test-all.sh` runs the logic suite and reports
individual failures. Screenshot and performance checks are separate. Some checks
need ffmpeg, mkvmerge or a graphical display.

All ten app icons are available in Settings and change branding inside the app.
The installed LG package icon stays fixed.

Icon artwork and generator sources remain in `design/icones-apoio` and
`design/logo-novo`. Upstream vendor sources retain their own platform portability
and licenses.
