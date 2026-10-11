# Standalone rewrite progress

This tracks implementation work, not approved feature deferrals. The requested
first prototype remains the scope in [the rewrite plan](standalone-rewrite.md).
An implemented service is not a completed screen or a verified TV feature.

## Account and catalog services

Implemented in standalone Rust:

- Fresh, versioned JSON storage with atomic replacement and private permissions.
  It does not import the previous application's data.
- Password login and TV code login, including anonymous bootstrap, UUID nonce,
  polling and account-token exchange. Credentials are not included in diagnostics.
- Persisted sessions, serialized token refresh, retry after HTTP 401, retention
  on transient refresh failures and rejection of late responses after logout.
- Account owner, profiles, addon inheritance, profile locks and PIN verification.
  New and legacy addon-sharing fields can coexist; the explicit field wins.
- The account's `addons` table, addon manifests and resource scopes, Home catalog
  ordering, enabled rows, search, details, episodes and source data.
- URL construction that preserves query credentials and encodes resource IDs and
  search extras. Metadata requests prefer English. Manifest data is shared between
  rows rather than copied per catalog.
- Bounded HTTP response bodies, request deadlines and redirect checks that keep
  account headers and request bodies within their origin.

Local evidence: 30 tests pass, including actual HTTP over loopback with synthetic
data. Host Clippy passes with warnings denied. No real login or provider request
has been made by these fixtures. The service code also cross-compiles with the
ARM SDK. The interactive screens are not connected yet; the preflight executable
does not exercise these services on ARM.

## Remaining prototype work

- Rust SDL2/GLES2 renderer, input/navigation, image and glyph caches.
- Login/profile screens and Home rows connected to the services above, then
  search, complete title/episode pages and manual source selection.
- Settings matching the reference appearance, including previews/illustrations.
- Continue Watching, library, account settings and their profile-scoped sync.
- Direct webOS playback, resume, pause/seek and subtitle/audio selection.
- Live TV/EPG, existing IPTV providers and M3U import by provider URL.
- Trakt and Simkl integrations.
- Local graphical/service checks, dependency licenses, ARM runtime/ABI validation
  and isolated packaging before the first substantial TV prototype handoff.

The current executable is still a build/runtime preflight. ARM compilation is
not evidence of actual login, renderer, playback or TV compatibility. No new IPK
is presented as a standalone prototype at this stage.
