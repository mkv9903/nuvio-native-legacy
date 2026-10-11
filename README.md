# Nuvio Native — personal LG webOS fork

This fork retains the LG webOS app and a Linux desktop preview for testing its UI.
The native client uses C, SDL2 and GLES2. LG playback uses the TV's media pipeline;
Linux playback is stubbed.

Based on [Nuvio Native Legacy](https://github.com/iqui27/nuvio-native-legacy).
Upstream attribution and third-party licenses are retained.

The app is displayed as **Nuvio Native** and uses package ID
`space.mkv.nuvio.native`. It has a separate installation identity and starts
with fresh local settings, login storage, cache and logs. Existing app data is
left untouched. Signing into the same account can still restore account data.

Android, Samsung/Tizen, WebAssembly/VIDAA and macOS builds have been removed,
along with the previously deleted social, P2P, Discord and personal media-server
features. Update checks use releases from
[this fork](https://github.com/mkv9903/nuvio-native-legacy/releases). The announcement feed, donation
UI and log/report uploads have been removed. Logs, crash reports and diagnostics
stay on the device; the user guide and first-use explanations remain available.
Source translation to English is a separate, later step.

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
bash tools/linux.sh
```

To browse the UI without an account, use `bash tools/linux.sh --preview`.
The script builds for the local compiler's architecture, including amd64/x86-64
and ARM64. Build on the machine where you intend to run the executable.

Build without launching:

```bash
bash tools/linux.sh --build
```

The executable is `build/linux/nuvio-native`. Local data defaults to
`${XDG_DATA_HOME:-$HOME/.local/share}/space.mkv.nuvio.native`; preview mode adds
`-preview` to that folder. Set `NUVIO_DADOS` or `NUVIO_DATA` to override it.
Navigate with arrows, Enter,
Escape or the mouse.

## LG webOS build

Build the SDK image and compile/package without deploying to a TV:

```bash
docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
bash tools/arm.sh --build
bash tools/arm.sh --ipk --build
```

Packaging requires `ares-package`. Configure server/API values through
`local.properties` or the variables read by `tools/env.sh`; keep credentials out
of version control. The SDK includes libass for styled subtitles.
See the [installation guide](INSTALL.md).

LG playback uses the TV's native player. The app does not bundle FFmpeg or
convert DTS audio, and there is no experimental Dolby Vision MKV pipeline.
Audio and video format support depends on the TV. The Dolby Vision and Dolby
Atmos source preferences affect source selection, not decoding.

`tools/arm.sh` without `--build` deploys to the configured rooted TV.
Use `--build` for local verification.

LG data defaults to `~/.space.mkv.nuvio.native`, with an app-specific fallback
under `/media/developer/temp/`. Local logs use
`/tmp/space.mkv.nuvio.native.log`. You will need to sign in on the new installation.
The updater uses this fork's GitHub releases and will offer a compatible LG
package when a newer release is published with an IPK asset. Until then,
manual update checks may report that a release could not be retrieved.

## Checks and maintained inputs

Tests live in `tests/`. `bash tools/test-all.sh` runs the logic suite and reports
individual failures. Screenshot and performance checks are separate. Some checks
need ffmpeg, mkvmerge or a graphical display.

The user guide is generated from `design/guia-de-uso/guia.json`:

```bash
python3 tools/guia_inc.py
```

All ten app icons are available in Settings and change branding inside the app.
The installed LG package icon stays fixed.

Icon artwork and generator sources remain in `design/icones-apoio` and
`design/logo-novo`. Upstream vendor sources retain their own platform portability
and licenses.
