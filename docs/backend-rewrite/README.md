# Rust backend rewrite

Active branch: `codex/rust-backend`, starting directly from `main` at `eff1a0b9`.
The former `rust-rewrite` branch is an inactive backup at `326077d5`.
No implementation from either discarded rewrite is carried into this branch.

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
boundary, provider loading and complete catalog title models pass 141 tests. The C login/profile screens and addon view layouts
also pass two Rust driver tests under Xvfb. The five relevant C reference groups passed
before their test cases moved to Rust. See [test coverage](test-coverage.md).
The Linux app builds and starts under Xvfb using disposable isolated preview
data. Retained C checks pass for Continue Watching/profile content, sync ordering,
account settings, lists, saved tabs, retained playback and release text.

The packaged Home-order checkpoint ARM build and isolated `space.mkv.nuvio.rusttest_2.1.0_arm.ipk` pass
package verification: exact runtime allowlist, test identity, executable mode,
no private/sample data or symbols, soft-float ABI and imported glibc versions
no newer than 2.12. Matching symbols, hashes and application source commit are
stored locally under `build/backend/webos`. Addon-service and edit-worker
compatibility builds are archived separately under its `checkpoints` directory;
they are not new IPKs or manual-test handoffs.

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
