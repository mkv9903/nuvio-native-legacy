# Rust rewrite

Work branch: `rust-rewrite`, created from `main` at
`eff1a0b9` (app version 2.1.0) on October 11, 2026.

## Starting documents

- [Antigravity's architecture evaluation](antigravity-architecture-review.md):
  an unchanged copy of the supplied root `RUST_ARCHITECTURE_REVIEW.md`.
  Root Markdown files other than the existing allowlist are ignored by Git;
  this copy preserves the starting point in a versionable location.
- [Architecture review and proposed migration](architecture-review.md):
  repository evidence, corrections, platform risks, and decisions for discussion.
- [First validation](first-validation.md): establish the C baseline, prove the
  hybrid Rust/C runtime, and migrate the first small module.
- [Testing the isolated hybrid](testing.md): build/run commands, separate app
  identity, diagnostics, and the Linux/physical-TV checklist.
- [Physical-TV validation](tv-validation.md): evidence from both uploaded TV
  logs, confirmed behavior, remaining coverage and observed issues.
- [Add-on URL helpers](addon-url.md): the first migrated module, English naming
  map, C adapter, byte/buffer contracts and reproducible parity checks.
- [Migration order and tests](migration-order.md): measured module sizes,
  dependency-based batches, the next text-helper group and test/translation policy.
- [Existing LG rendering research](../ux-render-lg.md): useful interaction,
  performance, and device validation requirements. Historical proposals need
  checking against the current LG-only fork.

## Agreed scope

Rewrite the application's own code in Rust, with accompanying English
documentation. Translate names, comments, logs, errors, and UI text into English
as each module is rewritten. The UI is **English only**, as confirmed by the
user on October 11, 2026.

LG webOS remains the playback platform. Linux remains the desktop UI preview.
Retain the current fork's product scope and native TV playback behavior.
Native dependencies such as SDL2, GLES/EGL, Luna services, and subtitle
libraries still require integration; a Rust rewrite does not require rewriting
those libraries.

New internal English names must not silently change existing saved-data keys,
filenames, numeric values, or external API fields. Document compatibility
mappings and explicit migrations. Keep upstream attribution and license text.

## Accepted architecture and migration order

On October 11, 2026, the user selected **Rust + SDL2 + GLES2** for the finished
application. `glow` is the proposed graphics binding; its version and the SDL
binding strategy still need validation against the TV runtime.

1. **Establish the Rust build and bridge.** Prove cross-compilation and runtime
   compatibility, then link a Rust core static library into the existing C
   executable through a small, explicit C ABI.
2. **Migrate logic and services while retaining the C UI.** Move domain rules,
   parsing, providers, account/sync, storage, and background work in complete,
   tested flows. Keep the current SDL2/GLES2 UI and any necessary C platform
   adapters working on LG and in the Linux preview. Translate each migrated
   module and its documentation into English.
3. **Validate the Rust core with the existing UI.** Establish behavioral parity,
   data compatibility, resource limits, and reliable playback integration
   before beginning the main UI rewrite. Validate each increment throughout
   migration rather than postponing integration until this stage.
4. **Rewrite application UI and orchestration in Rust.** Port screens, focus,
   input, rendering, text/image caches, and the remaining platform integration,
   retaining the working Rust core. Remove replaced C application modules as
   each flow reaches parity. Native third-party libraries remain dependencies.

The hybrid is the migration stage; the final application owns its logic and UI
in Rust. Linux UI preview remains available through both stages.

## Current status

The first Rust core is linked into the existing C app through ABI version 1.
Rust 1.85.1 is pinned, with the `armv7-unknown-linux-gnueabi` soft-float target.
The Linux probe, graphical rendering/navigation smoke check, identity/data/log
isolation, ARM cross-link and release packaging checks pass. **Physical-TV
startup/probe validation passed** in both user-supplied logs on LG
32LK628BPTF, webOS 4.4.3, firmware 05.50.70. Rust allocation, worker spawn/join,
and file round trip/removal passed; the C UI rendered and the app exited
normally. The second log also confirms login/profile sync, native movie
playback, subtitle selection, pause/resume and completed seeks. The user
confirmed picture, audio, subtitles and navigation behaved normally compared
with the current app. **The initial bridge and targeted hybrid smoke checks
are sufficient to begin core migration.** The add-on URL helper port now runs
in Rust Test behind the existing C callers, with direct C/Rust parity and
service integration checks. See the [module record](addon-url.md). See the
[TV validation record](tv-validation.md) for a failed 4K trailer, frame stalls
and remaining lifecycle/persistence/performance coverage. This is an
experimental build against glibc
2.12, below Rust's official modern Linux baseline; each future dependency and
runtime facility needs validation rather than assuming full platform support.

`--rust-test` builds **Nuvio Rust Test** (`space.mkv.nuvio.rusttest`) with
separate data, logs, diagnostics, executables and symbol archives. TV builds
always stop before deployment. Add-on URL rules are migrated; the broader
add-on service and C UI remain in place. Broader service bridge contracts, dependency
versions, data migrations and UI redesign remain to be established.

## Proposed first milestones

1. Confirm the minimum TV/firmware, behavior inventory, and a reproducible C
   baseline with memory and frame measurements.
2. Prove that a pinned Rust toolchain can build and run with the webOS SDK's
   soft-float ABI and the TV's system libraries.
3. Establish the core module boundaries, feature checklist, English naming map,
   and C ABI bridge, including ownership, cancellation, and threading rules.
4. Migrate and validate one complete core flow at a time with the C UI, including
   its tests, persistence compatibility, and documentation.
5. After core parity, rewrite the UI in Rust while preserving SDL2/GLES2
   behavior, remote input, native video composition, and lifecycle handling.

## Future portability

Future cross-platform support is a design consideration, not added delivery
scope. Keep core rules independent of OS APIs, and isolate playback, graphics
context creation, input, storage paths, lifecycle, and packaging behind clear
boundaries. SDL2 supports major desktop/mobile platforms; GLES2 may be provided
natively or through a translation layer. Each platform still needs its own
build, native media integration, and validation. Current work targets LG and
the Linux UI preview.

References: [SDL2 platform support](https://wiki.libsdl.org/SDL2/Introduction),
[Windows GLES2 support](https://wiki.libsdl.org/SDL2/README-windows),
[ANGLE](https://github.com/google/angle).

Local and physical-TV evidence establishes the initial bridge and targeted
hybrid behavior. Broader regression coverage and comparable performance remain
to be validated as migration proceeds.
