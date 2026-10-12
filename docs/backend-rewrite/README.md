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
- Use versioned Rust JSON storage and migrate each rewritten area's existing
  test-app data. Migration must not read production-app data.
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
profile selection and PIN rules; account/profile isolation; and storage migration.

Profile-specific Home catalogs remain the next related capability. The known
profile-switch catalog issue is addressed when Rust owns catalog loading and its
profile generations, without a separate fix in the superseded C branch.

The user confirmed two account-outage rules: retain verified cached profile
locks and block unverified profiles; retain a confirmed cached shared-account
owner and otherwise wait/retry rather than using the signed-in member's ID.

## Validation status

Clean restart and scope decisions are recorded. The Rust account domain,
service client and versioned storage pass 26 tests. The five relevant C
reference groups also pass. See [test coverage](test-coverage.md).
C UI integration, worker/boundary verification and platform builds are next.
No newly rewritten flow is ready for TV testing yet. Previous TV evidence
validates the earlier experiment only.
