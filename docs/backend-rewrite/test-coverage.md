# Account and profile verification

Reference: `main` at `eff1a0b9`, archived locally under the ignored
`build/backend-reference` directory before implementation changes.
Reference tests run against fake transports or a local HTTP fixture. No real
account credentials are used by these tests.

| Existing C coverage | New Rust coverage |
| --- | --- |
| `sessao_email` password request, escaping, service errors and offline retry | `service` request/Serde/error cases; `account` transport failures; HTTP fixture added with UI integration |
| `sessao_fora` outages preserve credentials; actual refresh rejection clears them | `account` outage, retry and rejection cases |
| `sessao_corrida` concurrent RPC/table refresh and private token readers | `account` shared rotating refresh and concurrent readers |
| `sessao_parcial` malformed/truncated saves, anonymous sessions, malformed authentication and TLS failure | `storage` versioned JSON validation; `service` authentication cases; `account` TLS outage cases |
| `perfilsel` profile fields, selection rules, startup cache, PIN outcomes, primary-addon inheritance and logout | `storage`, `service` and `account` profile cases |

All five reference groups passed before integration. The current Rust suite
passes 40 tests plus the native UI driver. Additional coverage includes late replies after cancellation or
logout, stale refresh against a new account, disk-write failures, schema identity
and version validation, isolated storage, private atomic writes, configuration
key completeness, unverified PIN locks and shared-account owner handling.

Agreed corrections to C behavior: keep confirmed cached owners and locks during
outages; do not substitute the signed-in member for an unknown account owner;
block selection of a profile whose lock has not been verified. Primary-addon
inheritance uses the profile marked primary instead of assuming index 1.

The user subsequently replaced the migration decision: this is a fresh test app.
Rewritten areas read only their own Rust JSON. Tests cover ignoring previous C
and discarded standalone files; there is no migration implementation.

Real HTTP transport fixtures, worker scheduling and native ownership now have
Rust tests. The C login and profile screens also pass a Rust driver under Xvfb:
QR rendering, email actions, profile/PIN selection, number keys, blank-cell
navigation, pointer actions, preparation guards, cancellation and stale identity.
Preserved visual behavior checks cover focus and light interpolation, clamped
resume intervals, paused PIN-background clocks, sustained frame-pressure
reduction and reduced-motion effects. QR polling/expiry also ignore wall-clock changes.
Screenshots are retained in the ignored `build/backend/native-ui/screens` folder.
Retained C Continue Watching/profile-content and sync-ordering tests pass with
the new models and account adapter. Their assertions stay in C until those
capabilities are rewritten.

Retained list, saved-tab, retained-player and release-text fixtures use a separate
fake profile context. They cannot bypass authentication or PIN selection in the
application. Their C assertions remain until their respective capabilities move
to Rust. Existing temporary-directory contracts remain intact in these unit
fixtures; the actual app and native UI driver use the isolated Rust Test identity.
The account-settings fixture correction and future Rust cases are documented in
[settings follow-up](settings-follow-up.md).

ARM linking succeeds with imported glibc versions no newer than 2.12. The latest
source, final isolated package and first user TV test remain to be checked.
