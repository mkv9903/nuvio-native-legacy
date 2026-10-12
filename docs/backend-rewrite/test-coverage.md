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
passes 45 tests plus the native UI driver. Additional coverage includes late replies after cancellation or
logout, stale refresh against a new account, disk-write failures, schema identity
and version validation, isolated storage, private atomic writes, configuration
key completeness, unverified PIN locks and shared-account owner handling.

Agreed corrections to C behavior: keep confirmed cached owners and locks during
outages; do not substitute the signed-in member for an unknown account owner;
block selection of a profile whose lock has not been verified. Primary-addon
inheritance uses the profile marked primary instead of assuming index 1.
Additional parity cases verify that an opaque database identifier does not
discard a valid numeric profile index or its PIN-lock record. Numeric fallback
IDs retain their existing precedence; the Rust wire model uses Serde for both.

The user subsequently replaced the migration decision: this is a fresh test app.
Rewritten areas read only their own Rust JSON. Tests cover ignoring previous C
and discarded standalone files; there is no migration implementation.

Real HTTP transport fixtures, worker scheduling and native ownership now have
Rust tests. The C login and profile screens also pass a Rust driver under Xvfb:
QR rendering and poll/exchange, keyboard-to-Rust email actions, profile/PIN selection, number keys, blank-cell
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
application. All four fixture groups pass. Their C assertions remain until their respective capabilities move
to Rust. Existing temporary-directory contracts remain intact in these unit
fixtures; the actual app and native UI driver use the isolated Rust Test identity.
The account-settings fixture correction and future Rust cases are documented in
[settings follow-up](settings-follow-up.md).

The approved account-settings fixture also passes with the final linking helper.
Linux startup passes under Xvfb with disposable isolated preview data.
The final ARM build succeeds with imported glibc versions no newer than 2.12.
The generated IPK passes `tools/test-ipk.sh` against the exact runtime allowlist,
test identity and executable permissions. The first user TV test remains pending;
see [the first flow check](first-tv-check.md).

The first Fedora user check exposed an RPC record-format gap in the initial
fixtures. New cases cover the live one-row QR creation/polling arrays, object
variants, empty/ambiguous/scalar replies, authorization-code URL fallback,
HTTP 400 classification and configuration-source diagnostics. The anonymous
live probe succeeds; user email login remains pending diagnostic retest. See
[the regression record](login-regression.md).
