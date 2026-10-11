# Standalone Rust rewrite

On October 11, 2026, the user replaced the incremental C/Rust migration with
a standalone Rust rewrite. This is the active direction on `rust-rewrite`.
The previous bridge and migration records are historical checkpoints.

## Accepted scope

- All application logic and UI are newly written in Rust. Do not link any
  existing application C source or the earlier `nuvio-core` compatibility library.
- Native SDL2/GLES2 and LG media libraries remain allowed platform dependencies.
- Keep `space.mkv.nuvio.rusttest` and the Nuvio Rust Test title.
- Preserve the cleaned application's appearance, remote navigation and settings
  previews as closely as possible. The C app and its tests are references.
- The first prototype includes Nuvio login, profile switching, profile-correct
  addons/catalog rows, title details, source loading and manual source selection,
  and both movie and series playback. Remove source autoplay.
- Write new code, tests, documentation and UI text in English.
- Use local Linux testing and ARM build/ABI inspection until a substantial
  prototype is ready. Do not request TV tests for isolated foundations.
- Ask the user before deferring complex features or resolving product choices.
  Routine implementation, dependency selection, testing and checkpoint pushes
  are authorized. The old profile-switch fix is superseded by new Rust state ownership.

Additional decisions confirmed by the user:

- Include Live TV/EPG and the existing IPTV provider integrations, plus importing
  channels from a provider URL pointing to an M3U playlist. Local file upload
  and USB import are not required.
- Include Trakt and Simkl. Defer plugins and debrid account services.
- Include playback resume, episode selection, pause/seek and subtitle/audio
  selection. Defer advanced ASS/SSA styling and dual subtitles.
- Start with fresh Rust settings and login. Existing hybrid data does not need
  to be imported or preserved by a migration. Keep the test app ID.
- Implement the app's existing logic directly in idiomatic Rust, using crates
  such as Serde instead of reproducing its custom C utilities. Local tests
  continue during development; no module-by-module user testing handoffs.

No product decision is currently pending. Request confirmation before any
additional feature deferral or product behavior change.

The user also explicitly confirmed English-only UI and English identifiers,
comments, filenames, logs and documentation throughout the standalone rewrite.

## Build configuration

The standalone build reads the same nine application service properties as the
existing app: `NUVIO_SUPABASE_URL`, `NUVIO_SUPABASE_ANON_KEY`,
`TV_LOGIN_WEB_BASE_URL`, `TRAKT_CLIENT_ID`, `TRAKT_CLIENT_SECRET`,
`SIMKL_CLIENT_ID`, `SIMKL_APP_NAME`, `TMDB_API_KEY` and `SEEKR_API_KEY`.
The mapping is in `standalone/build_support/service_config.rs`, used by
`standalone/build.rs`. Tests compare it with `local.properties.example` so a new
documented property cannot be omitted silently. Configuration values are never
printed by the build script. Personal login and provider credentials are runtime
settings, not build properties. App version comes from the Cargo manifest.

## Initial local validation

The pinned crate graph builds with Rust 1.85.1 on Linux. All 11 foundation tests
pass, including profile ownership, auxiliary-vector parsing and service
configuration coverage. Host Clippy with warnings denied, shell checks and
formatting checks pass. The independent ARM executable also cross-links with the
SDK after providing the target's missing `getauxval` interface in Rust. This
interface reads Linux auxiliary-vector entries with fixed stack storage; it
is not an adapter to the old application. The target remains below Rust's
official GNU/Linux baseline, so physical-device compatibility is still unproven.
The current preflight ELF is ARM EABI5 soft-float and imports glibc symbols no
newer than 2.12. Repeat ABI inspection as features start using more libraries.

This first executable is a runtime/build preflight, not the requested TV
prototype. Do not hand it off for TV testing or claim completed app features.

Run `bash tools/rust-app.sh check` for the standalone tests and
`bash tools/rust-app.sh arm` for the independent SDK build. The `linux` command
currently runs only the runtime preflight; the graphical app is under construction.

## Foundations

`standalone/` is an independent Cargo project. Its dependency graph must not
contain the old Rust compatibility core. Start with explicit account/profile
state ownership: Home results carry the account owner, active profile, effective
addon profile and a monotonically increasing request generation. Clear old Home
rows on selection; reject responses from an earlier selection, including when
the user returns to the same profile. Shared addons do not imply shared Home state.

Keep networking and decoding outside the rendering thread. Use bounded work
queues, response limits and texture budgets. UI/GL resources belong to the main
thread. Platform-specific media owns its callbacks and shutdown lifecycle.

Candidate dependency versions are pinned in the standalone manifest, with an
independent lockfile and Rust 1.85.1. They are provisional until host and ARM
builds pass. SDL2 bindings must use the TV's native SDL; do not bundle a new SDL.
Use GLES2 shaders without Vulkan, GLES3-only calls or an extra UI framework.

The inspected SDK is GCC 14.2, ARMv7 GNU soft-float, glibc 2.12. Host and SDK
SDL versions do not prove the exact TV runtime version. Inspect imported SDL
symbols, loader, CPU attributes and glibc symbol requirements before packaging.
Previous hybrid TV success proves neither the new executable nor new dependencies.

Official references: [Rust SDL2](https://github.com/Rust-SDL2/rust-sdl2),
[glow](https://docs.rs/glow/0.13.1/glow/),
[ab_glyph](https://github.com/alexheretic/ab-glyph),
[image](https://docs.rs/image/0.24.9/image/),
[Serde JSON](https://docs.rs/serde_json/latest/serde_json/),
[ureq](https://docs.rs/ureq/3.3.0/ureq/),
[webOS media API](https://www.webosose.org/docs/reference/ls2-api/com-webos-media/).
The webOS OSE API is supporting reference; the target consumer TV has different
permissions and runtime behavior. Confirm native contracts against its SDK
and the known working integration, then implement them directly in Rust.

## Completion evidence

See [implementation progress](standalone-progress.md) for current coverage and
the remaining work toward the first prototype.

Track feature coverage explicitly rather than promising a line count. Retain
unit and service integration tests, synthetic account/profile fixtures, Linux
rendered screenshots/navigation checks, runtime memory observations, dependency
licenses, reproducible ARM artifacts and a clean, isolated IPK. Label build-only,
Linux runtime and physical-TV evidence separately. Do not claim feature parity
or physical-TV compatibility before it is exercised.
