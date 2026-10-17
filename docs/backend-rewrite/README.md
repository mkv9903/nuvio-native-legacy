# Rust backend rewrite

Active branch: `codex/rust-backend`, starting directly from `main` at `eff1a0b9`.
The former `rust-rewrite` branch is an inactive backup at `326077d5`.
No implementation from either discarded rewrite is carried into this branch.

For the current completion estimate, capability inventory and next feature
review, see [rewrite progress](progress.md). Older sections below preserve the
implementation and validation history of individual checkpoints.

## Agreed approach

- Preserve the C UI's appearance, layout, rendering and remote navigation.
  C models, interfaces and data access may change to consume the Rust backend.
- Rewrite complete backend capabilities using Rust models, standard collections
  and appropriate crates. Do not reproduce the application's custom C JSON
  reader/writer or constrain domain models to old fixed buffers.
- Keep the language boundary small: actions, immutable results and explicit
  ownership. C callers adapt to the Rust API. Temporary adapters may support
  C capabilities that have not yet been rewritten.
- Keep the isolated Nuvio Rust Test name and `space.mkv.nuvio.rusttest` identity.
  Production-app data must remain separate.
- Use fresh versioned Rust JSON for each rewritten area. Restore only this
  rewrite's own data. Do not import C files or discarded standalone files;
  this replaces the earlier migration decision.
  The user reaffirmed this rule: installation is treated as a new app, with
  storage private to `space.mkv.nuvio.rusttest`. Do not add compatibility readers,
  converters, production-directory searches or old-format fallback paths.
- Write all new Rust code, tests, logs, comments, documentation and filenames in
  English. Translate complete C UI components when touched; translate only the
  affected UI component in a large mixed file. C backend areas awaiting
  replacement do not need a separate translation pass.
- Preserve required external API keys and native library identifiers.
- Write migrated backend tests in Rust, covering the existing C test cases and
  additional domain, service, ownership, cancellation and resource-limit cases.
  Existing C tests continue to cover C capabilities that have not been migrated.
- Run local Linux checks after each complete area. Ask for the first TV test
  after the first complete Rust-backed flow, then use targeted TV checks for
  dependency, runtime, playback and platform changes.
- Push checkpoints regularly. Build and push an isolated IPK before a TV handoff.
  Do not deploy automatically.
- Before rewriting or integrating each capability, describe its existing
  features and pause for the user's keep/remove decision. Apply confirmed
  removals as part of that capability's rewrite.
- Retain the existing playback and subtitle features for now, including styled
  ASS/SSA, AutoSync, dual subtitles, PiP and seek thumbnails. The simplification
  discussion did not authorize removing them.

## First capability: account, sessions and profiles

The first rewrite owns configuration, account networking, session persistence,
password and TV-code login, refresh, cancellation/logout, account owner, profile
selection, PIN verification and primary-addon inheritance.

Rust owns domain state and asynchronous work. The C login/profile screens retain
their visuals while consuming Rust state. Unmigrated C sync/provider callers can
use a thin authenticated-request adapter until their capabilities move to Rust.

Before replacing code, establish the `main` behavior and translate its relevant
tests into an English case inventory. Cover password escaping and error mapping;
server outage versus rejected credentials; incomplete saved sessions; concurrent
refresh; token readers; retry; late login/refresh replies; shared-account owner;
profile selection and PIN rules; account/profile isolation; and versioned storage.

Profile-specific Home catalogs remain the next related capability. The known
profile-switch catalog issue is addressed when Rust owns catalog loading and its
profile generations, without a separate fix in the superseded C branch.

The user confirmed two account-outage rules: retain verified cached profile
locks and block unverified profiles; retain a confirmed cached shared-account
owner and otherwise wait/retry rather than using the signed-in member's ID.

## Validation status

The Rust account domain, HTTP client, worker, native boundary, platform helper,
Home ordering, addon networking/storage, edit merging and the native addon
boundary, provider loading, complete catalog title models and scene
storage/publication pass 196 tests. Forty-five additional Rust driver tests cover
the retained C account/addon screens, view layouts, Home-order publication,
source/subtitle workers, title lifetimes and integrated catalog/profile paths.
All 37 native catalog/title/capture cases also pass ASan/UBSan. The five relevant
C reference groups passed
before their test cases moved to Rust. See [test coverage](test-coverage.md).
The Linux app builds and starts under Xvfb using disposable isolated normal-mode
data. The addon runtime checkpoint also checks retained sync ordering, account
outages, source cache/status/retries, progressive source publication, subtitle
selection and transitional manifest cache behavior. Earlier checkpoints checked
Continue Watching/profile content, account settings, lists, saved tabs, retained
playback and release text.

The packaged Home-order checkpoint ARM build and isolated `space.mkv.nuvio.rusttest_2.1.0_arm.ipk` pass
package verification: exact runtime allowlist, test identity, executable mode,
no private/sample data or symbols, soft-float ABI and imported glibc versions
no newer than 2.12. Matching symbols, hashes and application source commit are
stored locally under `build/backend/webos`. Addon-service and edit-worker
compatibility builds are archived separately under its `checkpoints` directory;
they are not new IPKs or manual-test handoffs. The addon-runtime compatibility
build at `0869329b` also passes ARM/ABI/glibc checks; it is archived without an
IPK, with the earlier matching package/runtime/symbol set restored.

The Home-order runtime at `e7385a83` passes 159 backend tests, eight native
Rust/C driver tests, retained C profile-cycle/outage/collection checks, and a
Linux build with disposable preview startup. Its ARM compatibility build passes
the soft-float/glibc checks and has matching executable/debug build IDs. It is
archived separately without an IPK; the canonical earlier package and its
matching runtime/symbol files have been restored and hash-verified.

The user's Fedora check found a QR response-shape regression, which was corrected
against the live anonymous service. The user has now confirmed that both QR and
email/password login work on Fedora. The revised native driver checks QR exchange
and the real keyboard-to-Rust email handoff. See
[the regression record](login-regression.md).
The new HTTP/TLS runtime has not yet been verified on the TV; previous TV evidence
validates the earlier experiment only.

The user approved a correction to a stale retained C settings fixture. Its
cause and the cases to replace in Rust are recorded in
[settings follow-up](settings-follow-up.md).

## Second capability: addons and Home catalogs

The user approved grouping account addon loading and edit persistence, manifest
and catalog loading, Home catalog order and profile isolation. Playback-source
and subtitle searches and general settings remain C capabilities; their data
access can adapt to Rust-owned results. See [the implementation record](catalogs.md).

Addon account state and edits now run in Rust in the actual app. The retained C
addon screen, guide actions and source/subtitle workers read pinned Rust
snapshots. The old C registry, file loader, account addon parsing and whole-list
uploads have been removed. Subtitle work is limited to four concurrent requests,
as confirmed by the user. The app now uses Rust catalog loading and private scene restoration/publication.
C Home assembly and enrichment remain as transitional consumers.

The native title foundation retains complete immutable Rust data across C
catalog copies and replacement. The user approved keeping the existing title
features and fixing perfect-score ratings. Home catalog production and private scene storage are integrated; other C
backend consumers remain for their own rewrites. See [title ownership](title-ownership.md).

Details, playback, live-channel and saved-menu selections now retain their own
title references. Deferred save/watched notifications clone and release an owned
title context. The touched notification components use English code/files/text
while retaining the existing island rendering. Retained playback holds its title
until the session is discarded; normal closure releases it after progress work.

Complete Home scenes now have a tested Rust model and private Serde JSON store,
covering artwork/cast/resume/seasons/availability and full provider values.
Native restoration now retains complete titles/rows and projects existing C
display fields. The application now captures/restores these scenes through the retained UI,
with complete Rust row ownership across layout projections. See [scene cache](scene-cache.md).

Live C title/row capture and the Rust scene publication service are now tested.
They preserve complete Rust data across display projections, scope memory/disk
work to the selected profile and configured addons, and reject stale restores
and saves. The live Home path now uses these guards, including scoped metadata and
Continue Watching callbacks; see [scene publication](scene-publication.md).
The implementation checkpoint `839e1b17` passes Linux build/preview and webOS
ARM soft-float/glibc 2.12 compatibility checks. Its build-only artifacts and
matching symbols are archived separately; no IPK or TV handoff was made.

Home assembly and Continue Watching's transitional title handoffs now retain
and release their Rust owners, including cancelled rounds, late results,
temporary saved-list tails and compacted-away titles. Four additional native
Rust cases verify these lifetimes. The Home catalog producer and private JSON scene cache are now active.
Expanded catalogs, search and the other C services still await their own rewrite.

The title/scene checkpoint at `ddc25bb9` also passes the Linux build and a
disposable preview startup, plus webOS ARM compilation, soft-float/glibc 2.12
checks and matching runtime/full/debug build IDs. Build-only artifacts are
archived under `build/backend/webos/checkpoints/ddc25bb9`. No new IPK was made;
the canonical earlier package/runtime/symbol set was restored and hash-verified.

The complete native scene restoration checkpoint at `c7198054` passes 184
backend tests, 21 native Rust/C tests, Linux compilation and disposable preview
startup. Its webOS ARM build passes soft-float ABI and glibc 2.12 checks, with
matching runtime/full/debug build IDs. Build-only files and hashes are archived
under `build/backend/webos/checkpoints/c7198054`. No IPK or TV handoff was made;
the earlier canonical package/runtime/symbol set was restored and hash-verified.

The integrated catalog candidate and manual checks are described in
[catalog runtime testing](catalog-runtime-testing.md). Use normal Linux mode
(`bash tools/linux.sh`) to test account catalogs; `--preview` skips account work.
The old binary scene-cache implementation and its C tests have been removed.

The integrated application checkpoint `cc351086` is packaged for manual tests.
Its Linux build/startup, 196 backend tests, 42 native tests and 34 native
ASan/UBSan cases pass. The webOS release/IPK passes payload, identity,
soft-float/glibc 2.12 and matching-symbol checks. Canonical artifacts and hashes
are under `build/backend/webos`; TV runtime testing is pending. The linked
catalog test guide records the exact source, package hash and manual cases.

The subsequent [profile catalog regression](profile-catalog-regression.md)
removes the user-rejected local sharing override and pins the Home worker
reader to its Rust job after the account wait.

Application checkpoint `204511dd` also passes the webOS ARM/IPK checks and
37 ASan/UBSan native cases. The current canonical test package contains this
profile-switch correction; source/build IDs, its SHA-256 and retest steps are
recorded in the linked regression guide. TV runtime remains unverified.
