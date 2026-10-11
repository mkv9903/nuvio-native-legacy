# C/Rust hybrid testing

This milestone links a small Rust runtime probe into the current C app. It
checks an allocation, a joined worker, and a temporary-file write/read/remove
on every launch. The initial physical-TV check passed; Rust Test now also uses
the [Rust add-on URL helpers](addon-url.md). The C UI, SDL2/GLES2 renderer and
webOS native playback are still in use.

## Separate installation

| Item | Rust Test | Current app |
| --- | --- | --- |
| Display name | Nuvio Rust Test | Nuvio Native |
| Package ID | `space.mkv.nuvio.rusttest` | `space.mkv.nuvio.native` |
| Linux executable | `build/linux-rust/nuvio-rust-test` | `build/linux/nuvio-native` |
| ARM executable | `build/rust/webos/nuvio-rust-test.arm` | `nuvio-proto.arm` |
| Default app log | `/tmp/space.mkv.nuvio.rusttest.log` | `/tmp/space.mkv.nuvio.native.log` on TV; terminal on Linux |
| ARM symbols/evidence | `build/webos-rust/` | `build/webos/` |

Rust Test starts with its own local login, settings and cache. Default TV data
candidates use the test ID: `$HOME/.space.mkv.nuvio.rusttest`, then
`/media/developer/temp/space.mkv.nuvio.rusttest`, then the packaged artwork's
`.space.mkv.nuvio.rusttest` directory. The log's `[data] writing to` line records
the actual writable directory.

`NUVIO_DATA` and `NUVIO_DADOS` are treated as parent directories in Rust Test:
the test ID is appended even when a production data path is inherited. The
test log override is `NUVIO_RUST_LOG`; it ignores an inherited `NUVIO_LOG`.
Startup/crash files and diagnostic controls also use the test ID. Signing into
the same account still uses the same remote account and its synchronized data.

Production update checks/install actions are disabled in Rust Test. Install
new test IPKs manually. The base production metadata, deployed binary and
build artifacts are preserved by `--rust-test`.

## Build prerequisites

Install Rust through rustup and the pinned toolchain/soft-float target:

```bash
rustup toolchain install 1.85.1 --profile minimal --target armv7-unknown-linux-gnueabi
```

The scripts build the Rust library on the host with the pinned toolchain,
then link it with the C compiler for each platform. Cargo files live in
`rust/`; build outputs and the task-local Cargo cache live under `build/rust/`.
No external Cargo crates are used yet. The host must already have the Linux
dependencies and, for TV builds, the `nuvio-webos-sdk` image and `ares-package`
described in the root README. Local server configuration continues to come
from `tools/env.sh`.

The SDK has **glibc 2.12**, below the [official modern Rust Linux minimum of
2.17](https://blog.rust-lang.org/2022/08/01/Increasing-glibc-kernel-requirements/).
This is an experimental compatibility build, not a supported glibc-2.12 Rust
target. The actual hybrid executable links with the SDK and requires symbols
no newer than 2.12; that evidence does not prove every Rust API or future crate
works. The build checks ARMv7, the soft-float ABI, `/lib/ld-linux.so.3`, required
libraries and symbol versions before packaging. The user's first-launch log
confirms that the initial Rust probe executes successfully on the physical TV.
The second log and user confirmation establish a targeted hybrid smoke check;
see [the validation record](tv-validation.md) for coverage and observed issues.
Pin changes and new dependencies
need the same build and physical-TV checks.

## Linux: test now

From the repository root on a Linux machine with a desktop session:

```bash
bash tools/linux.sh --rust-test --preview
```

This compiles and launches the window titled **Nuvio Rust Test - Linux UI
preview**. It stages clean resources in `build/linux-rust/preview-app/`,
including public sample catalog/artwork and fonts, excluding developer
accounts, settings and caches. Preview data defaults to
`${XDG_DATA_HOME:-$HOME/.local/share}/nuvio-rust-preview/space.mkv.nuvio.rusttest`.
The normal login mode, `bash tools/linux.sh --rust-test`, uses
`${XDG_DATA_HOME:-$HOME/.local/share}/space.mkv.nuvio.rusttest` instead.

For an already-built local executable, skip recompilation:

```bash
NUVIO_UI_PREVIEW=1 build/linux-rust/nuvio-rust-test build/linux-rust/preview-app/art
```

This direct command uses the runtime's default data directory rather than the
launcher's preview directory; both are scoped to the test ID.

1. Confirm the window opens and text/artwork render. Dismiss first-use dialogs
   using Back or Enter. If the fresh Home says "No active rows", enable rows
   in Settings under Home rows.
2. Use arrows, Enter, Back/Escape and the mouse. Open a title and Settings,
   navigate back, and open the log with F9.
3. Close/reopen the test app and check that a local setting persists. Confirm
   your regular app still has its own settings and account state.
4. Confirm the test log contains the success line below.

Linux playback remains stubbed. Software-rendered/headless frame timings are
not a performance benchmark for the TV.

## TV: targeted validation when needed

The initial runtime and targeted hybrid checks already passed. Pure URL rules
are primarily covered by Linux parity/integration tests; a new full playback
session is not required for every helper change. Repeat TV checks when changing
runtime dependencies/facilities, networking, storage/lifecycle, remote input,
rendering or native playback. The checklist below remains available for those
increments and optional spot checks.

Build a local IPK without any TV contact:

```bash
bash tools/arm.sh --rust-test --ipk
```

`--rust-test` always stops after building/packaging, including when `--build`
is omitted. Version 2.1.0 produces
`space.mkv.nuvio.rusttest_2.1.0_arm.ipk`. `--debug` keeps symbols in the executable
and adds `-debug` to the package filename; separate symbols are retained for
both profiles. `build/webos-rust/latest-release.txt` identifies the matching
symbol directory; `abi.json` and `build.json` record inspection and hashes.

Install using your existing IPK installation method, or the configured webOS
CLI device name (replace `YOUR_DEVICE`):

```bash
ares-install --device YOUR_DEVICE space.mkv.nuvio.rusttest_2.1.0_arm.ipk
ares-launch --device YOUR_DEVICE space.mkv.nuvio.rusttest
```

1. Confirm both **Nuvio Native** and **Nuvio Rust Test** remain installed. The
   test app should begin with fresh local state; sign in separately if needed.
2. Confirm startup reaches the UI and the Rust probe passes in the test log.
3. Navigate Home, Search, a title, sources and Settings with the remote. Play
   one familiar video; check pause/resume, seeking, audio, subtitles and Back.
4. Switch to another app and return. Close/relaunch Rust Test and check that
   its login/settings persist. Confirm the current app remains usable.
5. Repeat comparable navigation/playback scenes in the current app. Report
   any new lag, missing overlays, failed playback or unexpected exits.

The expected line on both platforms is:

```text
[rust-core] app=space.mkv.nuvio.rusttest ABI=1 probe=PASS status=0 checks=7 checksum=522240
[rust-core] modules=addon-url,stylized,text-cleanup,search-normalization
```

Use the red remote button or Settings > About and help > View the log on
screen. If startup fails, use your existing TV file/SSH access to read:

```text
/tmp/space.mkv.nuvio.rusttest.log
/tmp/space.mkv.nuvio.rusttest-anterior.log
/tmp/space.mkv.nuvio.rusttest-startup.txt
/tmp/space.mkv.nuvio.rusttest-startup-crash.txt
```

After the regular crash handler is armed, crash details also live in the test
data directory's `queda.txt`. Rust Test telemetry/control files use the prefix
`/tmp/space.mkv.nuvio.rusttest`, including `-fps.txt`, `-marcos.txt`, `-key`,
`-video`, `-shot-req`, `-shot.bmp`, `-quadros`, `-gfx-off`, `-acb`,
`-faixas.json` and `-plugins-local`. Existing production tools using
`/tmp/nuvio-*` control the production app. Diagnostic requests are consumed by
truncating their files rather than removing them.

Report the TV model/firmware, whether the probe passed, which check failed,
and the test log/startup/crash details if relevant. Current core migration and
its automated parity checks are recorded in [add-on URL helpers](addon-url.md).
