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
passes 117 Rust tests and two native driver tests. Additional coverage includes
late replies after cancellation or logout, stale refresh against a new account, disk-write failures, schema identity
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
live probe and real Linux app QR startup succeed. The native input case preserves
mixed case, quotes, backslashes and surrounding password spaces. The rebuilt ARM
IPK passes packaging and compatibility checks. The user confirmed both QR and
email/password login on Fedora. See
[the regression record](login-regression.md).

## Home catalog checkpoint

`backend/tests/catalog_order.rs` replaces the migrated C `catordem` and
`catordemcache` test programs. Its 11 Rust tests cover their reference cases plus
over 2,048 remote entries, over 6,144 local rows, complete Unicode/escaped/long
keys, malformed input, atomic private versioned JSON, ownership checks,
untrusted versions, symlinks and held native snapshots. C parser and persistence
implementations are removed; retained C consumers use small adapters.

The native text projection is shared per revision. C readers retain owned
snapshots instead of allocating every key on every renderer query. Account
storage now reuses the same private atomic-file helper; all account storage
tests still pass.

`backend/tests/addons.rs` adds 13 tests for the upcoming addon/manifest boundary:
80 installed addons with URLs beyond 2,047 bytes and names beyond 64 bytes;
canonical URLs with complete path/query configuration; resource URL escaping;
redacted diagnostics; authoritative empty lists and whole-list rejection;
root identities; exact resource names; root/string filters and independent
object-resource filters; long/many prefixes; 703 catalogs with late search
targets; required search versus required genre; channel types; malformed and
oversized responses. These models are not yet connected to the C addon/search
consumers, which still have their old limits until integration completes.

Retained C integration checks pass for `syncordem`, `synccolecoes`,
`colfileiras_sync`, `home_hero_layout`, `homepos`, `snapshot_falha` and
`desc_iniciar_corrida`. The sync fixture now gates its fake Home-order response
explicitly before testing cache-before-network behavior; it previously raced a
fast worker. The isolated sync cache fixtures use a temporary Rust Test directory.

`contaoffline_limites` cannot link because its retained fixture lacks saved-list
and account-journal doubles. The same missing symbols occur at the unchanged
reference `f8b4f72a`; it is not counted as a passing check here. Its unmigrated
C fixture needs repair when that coverage is used for subsequent integration.

## Account-addon service, storage and native snapshots

Twenty-one new Rust tests cover owner/effective-profile requests, pagination beyond
500 addons, complete Serde pushes, disabled entries and empty removals; stale
replies after switching profiles away and back; independent and primary-addon
contexts; failures on later pages; and unknown-owner/unverified-profile gates.

Storage cases cover fresh private JSON, long names and URLs, restart, separate
member/owner/profile identities, hashed filenames, explicit empty data, exact
edit acknowledgements, concurrent store handles, cache promotion on ack,
unknown versions and corrupt files, legacy-file exclusion, logout cleanup and
symlink rejection and consistent paired cache/journal restoration. Native cases cover large lists, late manifest catalogs,
shared snapshot lifetime after parent release, independent resource filters,
owned/encoded long request URLs and display NUL handling.

The retained C account/profile screen driver passes after adding private-content
generation tracking. The addon controller and C consumers are not yet bound to
these new services; their application-level limits and profile-row behavior
remain pending. Confirmed recovery and cross-device edit policies are recorded in
[the catalog implementation record](catalogs.md).

## Addon edit policy and worker

Twenty-seven additional Rust tests cover the confirmed merge policy, local
journals and the native action flow. Cases preserve phone-added providers,
remote names/order and removals; apply pending TV toggle conflicts; reinstall
only through an explicit install; serialize full URLs and Unicode; reject
unsupported actions; and retry intentions idempotently. Storage tests verify
compare-and-replace, unreadable journal protection and promotion of the uploaded
merged list, including additions absent from the original TV cache.

Backend and worker cases cover offline restart, failed GET without upload,
damaged journal with no network or file replacement, recovery retry, damaged
cache with a valid journal, write/ack failures, retained RAM edits, edits saved
after switching profiles, stale GET/POST replies, newer actions during upload,
authoritative empty lists, separate manifest/list revisions, conflicts with
another store handle, bounded latest-profile queuing, actions during in-flight
work, detached drop and logout during upload. Native actions cover load, toggle,
install, full URLs, stale/null/invalid inputs and held snapshot lifetime.

The two native driver tests cover the retained account/profile screen behavior
and all C/Rust addon view sizes/field offsets. C supplies layout measurements;
the assertions are in Rust. The app's addon C consumers still need integration;
these checks are not a manual TV or Linux account/catalog handoff.
