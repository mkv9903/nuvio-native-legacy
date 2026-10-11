# Review of the Rust architecture starting point

Reviewed October 11, 2026, against `main` at `eff1a0b9` (2.1.0).
The [supplied evaluation](antigravity-architecture-review.md) is preserved
unchanged. This document distinguishes repository evidence, external framework
capabilities, and proposals that still need measurement.

## Assessment

The evaluation correctly prioritizes a transparent native video plane,
directional remote input, GLES2-compatible rendering, and constrained memory.
It is useful for shortlisting approaches, but does not yet establish that any
proposed Rust implementation runs on this TV. Its framework memory totals and
performance guarantees cannot be used as acceptance criteria.

The initial review shortlisted these candidates:

- **Rust + SDL2 + GLES2**, potentially using `glow`: carry forward known window,
  input, shader, and composition behavior while moving application ownership
  into Rust. This requires building and maintaining the UI components.
- **Rust + Slint on a custom SDL2 platform**: declarative layout may make UI
  development easier. Validate the selected renderer, custom platform, memory
  behavior, and native-video composition before adopting it.

Following discussion on October 11, 2026, the user selected **Rust + SDL2 +
GLES2**. The accepted migration retains the C UI while logic and services move
into a Rust core, validates that core with the working UI, then rewrites the
application UI and remaining integration in Rust. Linux UI preview is retained
throughout. See the [accepted migration order](README.md#accepted-architecture-and-migration-order).

Prove Rust/webOS compatibility first, preserve current SDL2/GLES2 behavior,
and validate each migrated flow. Slint is retained below as research context;
it is not part of the selected implementation. Keeping our C UI permanently
would leave the requested application rewrite incomplete.

## Evidence from this fork

| Area | Observed implementation | Consequence for the rewrite |
| --- | --- | --- |
| Platforms | Root README and `tools/linux.sh` / `tools/arm.sh` maintain LG native playback and a Linux UI preview. | Keep these two targets in scope; historical references to retired platforms do not add rewrite requirements. |
| Graphics | `src/main.c` requests GLES2 and 8-bit RGBA channels. `src/gfx.c` uses a scissored alpha-zero clear for `gfx_furo`. | Preserve context capabilities, clipping, blending, and video cutout semantics. |
| Compositor | `src/main.c` explicitly clears the Wayland surface's opaque region through a runtime-loaded function. Its comments describe SDK/runtime SDL layout differences. | Transparent widget colors alone do not establish TV video visibility. Audit binding layouts and preserve the native surface behavior. |
| Media | `src/video.c` loads Luna-service and GLib functions dynamically and calls `luna://com.webos.media`. | Playback is a platform subsystem with callback, lifetime, thread, and shutdown requirements. |
| Networking | `src/net.c` loads `libcurl.so.5`, then `.4`. `src/net.h` defines limits, cancellation, redirect rules, and generation checks. | Preserve these contracts when selecting a Rust HTTP implementation; avoid introducing incompatible shared-library dependencies. |
| Memory | `src/perfiltv.c` selects a 48 MB texture budget below 1,200 MB total RAM, with a 64 MB settings ceiling. | Start with the current low-memory policy and measure total memory separately from texture allocation. |
| UI language | `src/idiomacod.h` has `IDIOMA_N = 1`; `src/idioma.c` maps Portuguese source strings to English. | The maintained UI already exposes English only. Old comments describing dozens of languages are stale. |
| Saved intent | `src/account_pending.h` describes an on-disk, per-user journal for watched and saved actions, with retry and reconciliation. | Preserve offline intent and account/profile isolation during the rewrite. |
| UI coupling | `home.c` and `detail.c` depend on catalogs, providers, settings, playback, and sync modules. | Extract explicit use-case boundaries; replacing a JSON parser alone does not separate UI from business logic. |

The inspected local `nuvio-proto.arm` is ELF32 ARMv7, EABI5, soft-float, with
interpreter `/lib/ld-linux.so.3`. Its recorded attributes include VFPv3 and NEON;
its versioned glibc references include `GLIBC_2.4` and `GLIBC_2.7`. This describes
that artifact, not the TV's installed glibc version or a proof of its minimum
hardware compatibility.

The top-level `src/*.c` files contain 158,110 lines including comments. The seven
files `home.c`, `detail.c`, `guia.c`, `gfx.c`, `tex_cache.c`, `player.c`, and
`main.c` account for 28,948. The original's "47,000 lines of UI" needs a defined
file inventory; neither count is an estimate of rewrite effort.

## Corrections and missing proof

### Memory and performance

Treat all per-framework RAM totals in the original as unmeasured estimates.
They omit or do not explain framebuffer allocations, temporary image decode
buffers, CPU/GPU copies, font rasterization, thread stacks, allocators, network
concurrency, and native-library overhead. The table and detailed sections also
give different totals for the same approaches.

For scale, a single 1920 x 1080 RGBA image contains 8,294,400 bytes (7.91 MiB),
before overhead. Two or three equivalent full-size color buffers account for
15.82 or 23.73 MiB; the actual compositor and driver allocation must be measured.

Comments in `src/perfiltv.c` cite historical low-memory LG sessions around
125 MB RSS with about 25 MB of textures, and 188–198 MB RSS near a 64 MB texture
budget. These are historical reports described in source, not measurements
performed during this review. They show why a 55–85 MB full-app estimate is
not established by asset arithmetic. The comments explicitly label presumed
memory-related terminations as a hypothesis without system logs.

Record process RSS/PSS where available, texture/staging allocation, system
memory, cache state, frame distributions, and playback state separately. Do
not sum overlapping process/shared/GPU counters. Neither Rust, raylib, nor
Slint guarantees 60 FPS or eliminates stutters. Define thresholds against the
same TV, scene, artwork, and active playback baseline.

### Slint needs a platform integration, not just layout markup

Slint documents custom platform and window-adapter interfaces. SDL2 is not a
built-in backend in its documented backend list; the proposed SDL2 platform
must be implemented and maintained. Its renderer documentation includes a
GLES2 route through FemtoVG in LinuxKMS, which supports keeping Slint on the
shortlist. That route does not establish a working webOS SDL2 integration, and
LinuxKMS must not be substituted for the TV compositor without further proof.
Sources: [Slint platform API](https://docs.rs/slint/latest/slint/platform/index.html),
[backends and renderers](https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backends_and_renderers/),
[LinuxKMS renderer capabilities](https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backend_linuxkms/).

Pin a Slint version and renderer. The proof must include alpha preservation,
non-opaque Wayland surface setup, the native video rectangle, overlays and
subtitles, remote focus, timers/redraw scheduling, bounded image caching,
background/resume, and context/resource recovery. Inspect the pinned Rust SDL
bindings against the TV ABI rather than copying the existing layout workaround
unexamined.

Slint's license options include GPLv3 for embedded applications; its
royalty-free proprietary option excludes embedded systems. This repository
already contains GPLv3 license text. Preserve notices and confirm the selected
release's license terms when selecting dependencies.
Source: [Slint's license document](https://github.com/slint-ui/slint/blob/master/LICENSE.md).

### Other framework conclusions need narrower wording

- **Raylib:** its upstream SDL platform implementation makes an SDL route
  plausible, but a webOS build, native surface transparency, and runtime ABI
  still require proof. The application still needs focus management and text
  input. Source: [raylib SDL platform source](https://github.com/raysan5/raylib/blob/master/src/platforms/rcore_desktop_sdl.c).
- **Iced:** "requires Vulkan" is inaccurate. It also has a software renderer.
  That capability does not demonstrate suitable webOS window integration or
  performance; keep it outside the initial shortlist for those unresolved
  reasons. Source: [Iced software renderer](https://docs.rs/iced_tiny_skia/latest/iced_tiny_skia/).
- **egui:** poor fit for the desired TV presentation is a product/engineering
  assessment, not evidence that custom directional focus is impossible.
- **Web UI:** the claimed "95%" adoption figure and exact Chromium/Rust-daemon
  memory costs have no supporting measurements. A native companion also
  introduces packaging, service permissions, lifecycle, and IPC work. It
  remains outside the proposed native rewrite scope.

### Cross-compilation is an early milestone

The evaluation does not specify a Rust toolchain, target, linker, sysroot,
standard-library compatibility, or dependency feature set. The SDK container
is AArch64, while the app output is 32-bit ARM; those are different targets.

`armv7-unknown-linux-gnueabi` is a candidate based on the existing artifact's
soft-float calling convention. Do not choose `gnueabihf` simply because the CPU
has floating-point hardware. Rust's ARM documentation distinguishes these
calling conventions and requires a suitable cross-linker. Confirm the pinned
toolchain's libc/kernel requirements against the real TV, then link with the
webOS SDK and inspect the resulting ELF interpreter, required libraries,
symbol versions, CPU features, and FFI layouts.
Sources: [Rust ARM Linux ABI and cross-compilation](https://doc.rust-lang.org/rustc/platform-support/arm-linux.html),
[Rust ARMv7 target requirements](https://doc.rust-lang.org/rustc/platform-support/armv7-unknown-linux-gnueabi.html).

Use a runnable Rust binary to establish compatibility, not just a successful
`cargo check`. Do not assume a Rust static library removes libc dependencies,
or that a static musl executable can transparently use all TV-provided glibc
libraries. Validate HTTPS/TLS and Luna integration before choosing their Rust
dependencies.

## Proposed application boundaries

These are responsibilities, not a committed Cargo workspace layout:

| Boundary | Responsibilities | Existing reference |
| --- | --- | --- |
| Domain | Titles, episodes, sources, profiles, watched/saved intent, ranking rules; no UI or network ownership | `catalogo`, `streams`, `vistoep`, source preference modules |
| Services | Authentication, addons, metadata, debrid, Trakt/Simkl, IPTV/EPG, account reconciliation, plugins | `session`, `addons`, `debrid`, `trakt`, `simkl`, `sync`, `account_pending`, `epg`, `plugins` |
| Storage | Versioned settings, per-profile/user state, journal, catalog/art cache, legacy readers | `ajustes`, `data`, cache modules, pending journal |
| Platform | SDL window/input, webOS media/GLib/Luna, native library loading, diagnostics, lifecycle | `main`, `video`, `webosver`, `startup`, `crash`, `net` |
| UI | English text, screen state, focus/back navigation, rendering, texture/glyph ownership, subtitles/controls | `app`, `home`, `detail`, `guia`, `focus`, `pointer`, `text`, `gfx`, `tex_cache`, `player` |

Keep rendering and GL resource ownership on the UI thread. Services publish
bounded results tagged with account/profile/title generations; reject stale
results when applying them, including after a request has completed. Choose
bounded workers or an async runtime from workload evidence. Define shutdown,
cancellation, error handling, and buffer ownership before adding an FFI bridge.

Build a feature inventory before claiming parity: login/refresh/logout,
profiles and sync, catalog discovery, search/detail/episodes, saved/watched
state, plugins/source resolution/debrid, live TV/EPG, playback/audio/subtitles,
resume/next episode, settings/themes/accessibility, updater, and local
diagnostics. Port behavioral tests and relevant fixtures alongside each flow;
retain visual and physical-TV checks for platform-specific behavior.

## English translation during migration

The user confirmed **English only** for the rewrite, including its UI.

- Write Rust identifiers, filenames, comments, docs, errors, and logs in
  English. Maintain a small old-module → new-module naming map as work lands.
- Write static UI labels and dynamic/plural/formatted text directly in English.
  Remove dependence on Portuguese phrases as runtime translation keys as each
  screen is migrated. Do not run catalog titles or other provider content
  through the UI label translation mechanism.
- Preserve Unicode/font handling for movie titles, names, search input, and
  subtitles. An English UI does not imply ASCII-only content or English-only
  subtitle/audio support.
- Keep external protocol fields and existing serialized keys, paths, and
  numeric identifiers compatible. Translate an internal name freely; changing
  `ajustes.txt` or a stored field needs an explicit reader/migration and data
  preservation check.
- Preserve upstream credits, license text, and third-party source attribution.
  Translate our documentation around them without changing license wording.

## Discussion and validation order

1. **Minimum hardware:** the supplied document names LG 32LK628BPTF, webOS Lite
   4.4.3, 1 GB RAM, kernel 4.4.84. Treat these as supplied assumptions until
   confirmed; do not infer free-memory or driver limits from the model alone.
2. **UI direction:** decide whether to preserve current appearance/interactions
   first or redesign them within the selected SDL2/GLES2 architecture.
3. **Platform proof:** build/run Rust, then prove SDL input, real native video
   visibility, overlay/subtitle composition, and suspend/resume on the target.
4. **Core migration:** define the Rust/C bridge's ownership, threads, and
   cancellation, then port complete logic/service flows with English docs and
   parity checks while retaining the C UI and necessary C platform adapters.
5. **UI migration after core parity:** port the UI to Rust, using realistic
   artwork, rapid remote input, text, and active video for validation. Compare
   memory and frame distributions with C; remove each replaced C application
   module when its flow reaches parity.
6. **Delivery:** preserve package/runtime-resource rules, local-only diagnostics,
   release symbols/build IDs, and required dependencies. Define test-build
   installation identity and data migration before installing a Rust build.

This review used source inspection, current upstream documentation, and local
ELF inspection. It did not benchmark frameworks, contact the TV, run a Rust
prototype, or establish that any proposed full-app RAM total is achievable.
