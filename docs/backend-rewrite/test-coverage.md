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

All five reference groups passed before integration. The earlier addon checkpoint
passed 145 Rust tests and seven native driver tests. Additional coverage includes
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
oversized responses. These models now feed the C addon screen and source/subtitle workers. C catalog,
guide and complete title ownership integration remains pending.

The earlier Home-order checkpoint passed retained C `syncordem`, `synccolecoes`,
`colfileiras_sync`, `home_hero_layout`, `homepos`, `snapshot_falha` and
`desc_iniciar_corrida`. Its C cache-before-network assertions have now moved to
Rust, with explicit barriers rather than scheduling-dependent sleeps.

Fourteen additional tests in `backend/tests/home_order.rs` cover the runtime
account RPC and selected profile despite primary-addon sharing; cache publication
before a held network reply; outages; immediate profile/owner/member isolation;
late replies and cache reads after context changes; authoritative empty rows;
same-scope optional preference merging and restart; damaged cache replacement;
usable results despite storage failure and retry; owner-less file exclusion;
unverified context gates; bounded latest-profile queueing and logout cleanup.

`backend/tests/native_home_order.rs` runs the real C frame bridge and renderer
accessors from a Rust driver. It verifies complete long keys, hidden flags,
preferences, no extra revision for identical responses, empty target state
while new-profile requests wait, explicit refresh, held snapshot lifetime and
logout. The native UI suite now has eight passing tests; the backend suite has
159. Retained C `syncordem`, `contaoffline` and `synccolecoes` also pass for this
runtime checkpoint. C sync asserts that Home-order fetching no longer uses it.

The collection fixture reused `syncordem.c` but redeclared its `addons_n` stub
and referenced a missing `ciclo` helper. Both issues are present in the prior
checkpoint's source. The fixture now uses the included stub and an explicit
`sync_iniciar`/frame-loop helper; all five original assertion scenarios pass.
This changes only test wiring. These retained collection cases must move to
Rust when account collections are rewritten.

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
generation tracking. The addon controller, screen/actions and source/subtitle readers now use these
services; complete catalog/title integration and profile-row behavior remain
pending. Confirmed recovery and cross-device edit policies are recorded in
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
the assertions are in Rust. These layout/account checks now include the real addon screen and remote
toggle/recovery behavior. They are not a manual TV or Linux catalog handoff.

## Provider HTTP, catalog titles and private document loading

Twenty-one additional Rust tests cover complete configured resource URLs and
encoded extras, external redirects without Nuvio headers, oversized bodies and
HTTP failures; root artwork precedence in both key orders, nested fallback,
metadata objects, long image URLs/IDs, independent provider/IMDb IDs, root
movie/series type precedence, English labels and metadata, missing/invalid
entries, authoritative empties, safe TMDB backdrops, raster logo fallbacks,
optional numbers, large catalogs, renderer quotas and whole-document syntax.

The cache and loader cases cover private atomic JSON, hashed paths, member,
owner and selected-profile isolation (including primary-addon inheritance),
configuration/extras separation, confirmed empties, invalid/cancelled saves,
corrupt versions/identity/symlinks, ignored legacy files, cleanup, cached values
before network, same-scope outage fallback, repaired corrupt caches and stale
responses after a profile round trip. These services are prepared for the C
integration; they are not yet application runtime paths.

Reference `tests/posterprov.sh` at `f8b4f72a` passes its poster-provider group,
but its catalog-art driver cannot link normally: unused discovery functions
reference missing `trakt_e_a_seguir`, `af_tmdb_logo` and
`af_tmdb_fundo_padrao` helpers. The unchanged original catalog-art assertions
pass with function/data sections and linker garbage collection, which drops
those unrelated unused functions. No reference source/assertion was modified.
Its root/nested artwork, metadata-object and long-URL cases are covered in Rust;
the active C tests remain until their runtime parsing path is replaced.

Manifest policy regression coverage checks incomplete/failed replies preserving
cache, explicit empty and source-only replies removing old catalogs, legacy
catalog-only providers, and independent full-resource type filters. That checkpoint passed 141 Rust tests; the subsequent runtime boundary adds
further coverage.


## Addon runtime and retained native workers

The runtime checkpoint passes 145 Rust backend tests and seven additional native
Rust tests (five addon worker/reader cases and two layout/screen drivers).
Coverage includes complete configured provider URLs beyond 7 KiB, eighty source
and subtitle providers, title IDs beyond the former C request buffer through the
synchronous source interface, independent HTTP headers, source retries retaining
their originating snapshot, pinned strings after profile changes, rejected late
source responses, four busy subtitle requests with the fifth queued, and cancelled
queued/late subtitles. The real remote UI action saves/uploads a toggle. An
unreadable-journal restart displays valid cached addons, blocks Enter and retains
the damaged bytes. Captures include `addons-rust-backed.png` and
`addons-recovery-paused.png` under the ignored native UI screens directory.

New backend/boundary tests guard manifest and source-health publications against
stale lists and profile round trips; distinguish metadata and list revisions;
preserve owned full manifest/subtitle URLs; reject malformed encoded UTF-8; and
verify native null/invalid inputs and public-host displays without credentials.

The removed C addon model/URL test programs are replaced by `addons`,
`addon_snapshot`, `addon_backend`, service/storage/edit/worker tests and the
native worker tests. Historical shell entry points call the Rust coverage.
The source-status, channel fallback and retry assertions remain in
`tests/source_status.c` until source search itself is rewritten. Retained C
source-cache, progressive-source and subtitle algorithm fixtures configure the
real Rust backend through a feature-gated fake account transport; they do not
recreate a mutable C addon registry. That support is excluded from app builds.

The retained checks pass for `syncordem`, `contaoffline`, `addonslista`,
`fontecache_vod`, `fontes_parcial`, `addons_legendas`, `manifesto_cache` and
`manifesto_wait`. Addon journal/cache/upload assertions were removed from the C
sync/outage fixtures because Rust owns those behaviors. The retained outage
fixture checks collection/library/watched fallback, ownership, retry/recovery
and logout, and proves C sync no longer requests the addon table. Transitional
C manifest fixtures now declare explicit resources instead of using deliberately
incomplete ID-only replies.

These checks caught and corrected a missing snapshot in the native source retry
job. The original retry, timeout and progressive-list assertions then passed.
The C manifest cache/orchestrator, complete title/guide data access and immediate
profile-change Home publication remain the next integration work. This checkpoint
is not a manual-test handoff and does not count TV runtime as verified.


The standalone retained `legendas_shot` capture is not counted as passing. Its
second-subtitle top-band pixel assertion fails under this host's Xvfb/llvmpipe
renderer with zero near-white pixels both before and after adding the second
subtitle. The unchanged C sources and assertions at `0aa01ab3` reproduce the
same failure. The temporary reference runner needed the existing Rust linking
helper enabled; no reference C assertions were changed. Reference/current
captures and logs are retained locally. This result does not establish a new
addon-runtime regression, and the capture fixture needs a separate investigation
before it can be used as a passing visual check. Synthetic subtitle capture
hooks remain available only in capture builds; normal app results still require
their current Rust context.

The ARM compatibility check caught a Linux-only preview variable used outside
its compile guard. `0869329b` corrects that guard; the webOS build then succeeds
with ARM soft-float, imported glibc versions no newer than 2.12 and matching
executable/full/debug build IDs. Its build-only artifacts and source/hash record
are archived under `build/backend/webos/checkpoints/0869329b`. No new IPK was
created. The existing matched `3d5ef46f` IPK/runtime/symbol set was restored and
its hashes verified. TV runtime and full Home/profile integration remain pending.

Retained test helpers now initialize/export their repository path even when a
parent runner supplies the already-built Rust library. The sync-order fixture
also passes with that prebuilt-library path, avoiding a missing registry source
in nested test runs.

## Native title ownership foundation

The title-ownership checkpoint passed 164 backend tests; the native Rust/C suite
passes 15. Five title tests verify complete values and separate provider/IMDb
identity, UTF-8 projection boundaries, null/malformed input, concurrent readers,
and perfect-score/near-perfect rating scaling. Three native title tests use the
actual C catalog and verify independent copies, release of final owners,
held selections through replacement, C artwork enrichment overlays and borrowed
text surviving in-place updates until frame retirement. No field-size or buffer
constant is used as a Rust domain limit. Four additional native cases cover the
real saved-menu/channel entry points, held Details/playback selections and
re-insertion by complete ID, deferred undo after releasing its source,
one-time execution, replacement/cancellation and clone failure. The retained
C playback-retention fixture passes its original same-session resume,
confirmation, timeout, identity-change and saved-source assertions.

The Linux application builds and remains running during a ten-second Xvfb
startup check with disposable preview data. Retained C `catcache`, `canaisidx`,
`catvida`, `catcorrida`, `retomar_copias`, `catconsistencia`, `cateps_corrida`,
`catfileira`, `catcachecorrida` and `psfundo` pass. `catvida`, `cateps_corrida`
and `catcachecorrida` also pass with AddressSanitizer/UndefinedBehaviorSanitizer.
The latter two initially reached LeakSanitizer's sandbox/ptrace restriction;
their unchanged checks pass outside that restriction, with sanitizers enabled.

These C assertions remain because the application catalog producer and scene
cache have not yet moved to Rust. Raw binary writing refuses Rust-owned titles
so opaque addresses cannot reach disk. This foundation is not a complete
catalog rewrite or a Linux/TV handoff; see [title ownership](title-ownership.md).

## Rust Home scene model and storage

The backend suite now passes 178 tests; the native suite still passes all 15
with the new Rust library and the unchanged C UI library. Fourteen scene-cache
tests cover complete metadata, artwork variants/logo evidence, cast/person IDs,
seasons, resume, subscription/rental/purchase and membership fields. Full IDs,
names, image/provider URLs and row keys round-trip, with arrays beyond the old
C limits. Tests preserve loading versus confirmed-empty states and authoritative
empty scenes, and reject invalid windows/identities/request values without
replacing a valid file.

Storage coverage includes member/confirmed-owner/selected-profile scope,
primary-addon inheritance, private permissions and hashed filenames, schema/
app/language checks, damaged files and symlinks, cancellation after temporary
write, late cache reads/errors, aggregate byte limits, concurrent store handles,
selective cleanup and exclusion of old C files. JSON serialization has a bounded
output buffer; strings are not silently truncated. The shared atomic-file helper
retains its previous behavior for account/addon/Home-order callers; their full
regression suite passes after adding the optional pre-rename context guard.

The scene layer is not yet the application cache. Native scene capture/restore,
remaining C copy ownership and actual catalog orchestration still need to be
connected before removing the C binary routines and their retained test programs.
See [the storage record](scene-cache.md).

The complete `ddc25bb9` source checkpoint builds for Linux and remains running
during a ten-second Xvfb preview check with disposable data. Its webOS ARM build
passes soft-float ABI and imported glibc <= 2.12 checks, with identical executable/
full/debug build IDs. Build-only artifacts and the source/hash record are archived
under `build/backend/webos/checkpoints/ddc25bb9`. The canonical earlier IPK and its
matching runtime/symbol files were restored and hash-verified. TV runtime and
complete catalog/profile integration remain unverified.

## Live rating conversion

All three retained metadata paths now delegate rating conversion to Rust. The
backend suite still passes 178 tests, with extended native-math edge cases in
the existing title test. The native suite now passes 16 tests: the new case
executes the real C catalog reader, detail enrichment and partial update through
the Rust helper. Perfect numeric/string 10 and percent 100 remain 100%, 9.95
rounds to 100%, 8.5 remains 85%, and missing/invalid partial values retain the
prior score. This prevents later detail enrichment from reintroducing the bug.

The live fix at `c1f58ce5` also builds for Linux and webOS ARM. Its ARM artifact
passes soft-float/glibc <= 2.12 checks and matching runtime/full/debug build IDs;
build-only files and hashes are archived under its `checkpoints` directory.
The earlier canonical IPK/runtime/symbol set was restored and hash-verified.
No new IPK was produced and no TV runtime test was performed.
