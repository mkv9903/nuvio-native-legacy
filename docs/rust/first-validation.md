# First validation before migrating features

The first milestone is a **working C application that calls a small Rust core
on Linux and LG webOS**. The existing C UI, renderer, native playback, and data
formats remain the reference behavior during this milestone.

## 1. Establish the existing baseline

Build the current Linux preview and webOS package with the existing scripts.
Record the commit/build ID and known warnings or test failures. Keep the
existing package resource allowlist and release symbol/archive checks.

On the TV, record startup, idle Home, repeated navigation, native playback
with controls/subtitles, and background/resume. Compare memory and frame timing
using the same scenes and cache state. Existing source comments and estimates
are not a substitute for this baseline.

## 2. Prove the Rust build and C interface

- Pin a toolchain and match the webOS SDK's ARMv7 soft-float ABI, libc, loader,
  CPU features, and native libraries. The installed ARM Rust target observed
  during preflight is `armv7-unknown-linux-musleabihf`; it does not match the
  existing application's GNU/soft-float build.
- Compile a minimal Rust `staticlib` and link it into a C test executable,
  then the current app. Call an exported function so linker garbage collection
  cannot turn the test into an app that never uses Rust.
- Exercise a small allocation, thread creation/join, and temporary-file round
  trip in the compatibility probe. A numeric greeting alone would not exercise
  the runtime facilities our core needs.
- Define C ABI types, buffer ownership, error reporting, and panic behavior.
  No Rust panic may unwind into C. Allocations must be released by their owner
  or through an explicit owner-provided release function.
- Inspect the linked ARM executable's interpreter, required libraries, symbol
  versions, and ABI, and confirm it actually runs on the target TV.

The static library exercises the intended hybrid architecture. A separate
Rust-only executable can help diagnose compatibility, but is not sufficient
evidence for the final C/Rust link.

## 3. Validate the hybrid app without migrating a feature

Confirm the Linux preview launches and navigation works. On the TV, repeat
the baseline startup, navigation, playback/overlays/subtitles, and lifecycle
checks with the Rust library linked and called. Compare resource usage and
release packaging. This proves the platform and interface before changing
application behavior.

HTTPS and Luna remain on their current C implementations at this stage. Their
replacement needs separate integration checks when those modules are migrated;
linking a Rust probe does not establish compatibility for future dependencies.

## 4. Migrate the first small module

Start with **add-on URL helpers**: normalization, query preservation, path/ID
encoding, and length/truncation rules. These have concrete existing behavior
and regression cases, and can be checked using synthetic URLs without account
credentials or live provider requests.

Retain the C callers through a narrow adapter, write the Rust implementation
and its documentation in English, and verify it against the existing cases.
The broader `addons.c` discovery/workers are a later migration. Then expand
into parsing/models and complete service flows; account persistence and native
playback integration require their own contracts and validation.

## Completion evidence

Before migrating subsequent features, retain a reproducible build recipe, the
C ABI header, Linux probe results, matching ARM build artifacts and inspection,
physical-TV probe/hybrid results, and first-module parity results. Clearly label
build-only, host-only, and physical-TV evidence.

## Validation results — October 11, 2026

| Check | Result |
| --- | --- |
| Current Linux app, `bash tools/linux.sh --build` | Built successfully; existing compiler warnings remain. This was build-only, not a graphical runtime test. |
| WebOS resource staging, `bash tests/webos_payload.sh` | Passed. |
| Release symbols, `bash tests/split_symbols.sh` | Passed. |
| Existing add-on URL cases, `bash tests/addonurl.sh` | Passed after repairing missing statistics test doubles in the existing harness. Application behavior was not changed. |
| SDK/toolchain compatibility | SDK image `linux/arm64`; GNU/soft-float ARMv7, glibc 2.12. Pinned Rust 1.85.1 and `armv7-unknown-linux-gnueabi` build and cross-link successfully. The SDK's libc is below Rust's officially supported baseline. |
| Native Rust/C probe, `bash tests/rust_core.sh` | Passed allocation, worker spawn/join, file round trip/removal, ABI layout, invalid arguments and I/O failure checks. |
| Identity isolation, `bash tests/rust_identity.sh` | Passed real data/log code with inherited production data/log overrides. Production settings remain unchanged. |
| Linux hybrid, `bash tools/linux.sh --rust-test --build` | Built successfully. Rust Test stages its own public preview resources, excluding account/settings/cache files. |
| Linux graphical smoke check | Passed under Xvfb with Mesa llvmpipe: Rust startup probe, rendered screenshots, consumed Down/Right/Back inputs and visible screen changes. This is functional evidence, not TV performance evidence. |
| ARM hybrid, `bash tools/arm.sh --rust-test --ipk` | Release IPK built: `space.mkv.nuvio.rusttest_2.1.0_arm.ipk`. Separate debug symbols, build IDs, hashes and ABI inspection retained under `build/webos-rust/`. No TV contact. |
| ARM executable inspection | ELF32 ARMv7 EABI5 soft-float; `/lib/ld-linux.so.3`; required glibc versions through 2.12. Rust is statically linked; SDK SDL2/GLES2 remain dynamic. This does not prove physical-device execution. |
| Build-profile regressions, `bash tests/arm_build_profiles.sh` | Passed production release/debug/runtime selection and isolated Rust package identity/title, separate binary/symbol paths, preserved production outputs and implicit build-only behavior. Uses a mock container/package tool; actual SDK build is listed separately. |
| Shell scripts / local log regression | ShellCheck on changed scripts and existing `tests/log_local.sh` passed. |
| Physical-TV Rust startup probe | Passed in the user's first-launch log, `space.mkv.nuvio.rusttest.log`: ABI 1, status 0, checks 7, checksum 522240. Confirms Rust allocation, worker creation/join, and temporary-file round trip/removal on LG 32LK628BPTF, webOS 4.4.3, firmware 05.50.70, ARMv7 kernel 4.4.84. No login or other user flow was tested. |
| Physical-TV C UI startup/exit | The same log shows a Mali-470 MP GLES2 context, a 1280x720 drawable, a rendered first frame, and normal teardown. This is one brief startup session, not a steady-state performance comparison. |
| Physical-TV targeted hybrid smoke check | Second uploaded log confirms login/profile sync, remote navigation, native HEVC MKV playback, subtitle selection, pause/resume and two completed seeks. The user confirms picture/audio/subtitles/navigation behaved normally compared with the current app. No Rust panic/process crash is logged; teardown is normal. See [TV validation](tv-validation.md). |
| Physical-TV issues and remaining coverage | A selected 4K trailer fails with native error 601, some frame stalls/artwork failures/diagnostic dump failures occur, and a delayed seek warning recovers. Causes and C-only parity are not established. Background/resume, post-login persistence, longer sessions and comparable performance remain follow-ups. |
| First feature migration | Add-on URL rules now run in Rust Test behind the existing C callers. 46,786 direct C/Rust comparisons, install/export/reload/request integration, Linux graphical smoke check and ARM build/ABI/package inspection pass. See [the module record](addon-url.md). The uploaded TV logs precede this port; physical-TV execution of the new helpers is not claimed. |

This is a limited preflight, not a full-suite or physical-device certification.
