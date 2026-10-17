# Search and expanded catalog browsing

Feature review completed on 2026-10-17. The user removed People search from
both Search and Spotlight, and kept all reviewed expanded-browsing features.
Retain the remaining Search features, Details cast information and filmography
navigation. Text handling and recent-query storage are integrated at `e44e99d3`;
provider search and expanded browsing remain in progress. The verified package
and focused manual checks for this checkpoint are recorded below.

## Existing features for review

Search combines built-in Nuvio movie/series catalogs, optional Cinemeta fallback
and installed addon catalogs that declare search support. It also matches titles
already loaded locally. Results arrive progressively in catalog/provider rows,
with a Best Result card. Short/empty queries show Popular suggestions. The
former People results (loaded cast and TMDb person-name search) are removed
from both search surfaces. Details cast and filmography navigation remain.

The device remembers ten confirmed searches per profile, newest first. Repeated
queries move to the front; individual removal and clear-all are supported, and
logout removes history. Recent-query storage and text normalization now run in
Rust through both retained C screens. The discarded C text files are never read.
Provider search, query-input ownership, grouped result assembly and expanded
browsing are still C services awaiting their next integration step.

Expanded browsing supports addon catalogs, genre filters and near-end page
loading. It deduplicates overlapping pages, stops on confirmed exhaustion, retains
loaded results after a failed page, supports retry and restores the loaded list
when returning to the same source. Collections add source tabs, direct TMDb and
Trakt list/discovery sources, ranking order and chronological filmographies.
Opening titles, poster actions, the existing information panel and navigation
back to the same position are part of the retained C screen behavior.

Keep the existing on-screen/physical keyboard and platform text-entry actions.
Use English UI/code/comments/names as already agreed. Valid Unicode titles,
provider IDs and query text remain data; providers receive the original query,
while local matching uses normalized text. UI appearance and navigation are not
redesigned in this milestone.

## Approved Rust boundary

Move search target discovery and eligibility, query state, provider requests,
parsing, grouped immutable results, local matching and recent-query storage into
Rust. Replace the C fixed target/query/request buffers with Rust models and small
native actions/views. Bound background workers and completed replies for the TV.
Profile, addon, query and request generations must reject obsolete results.

Move expanded-list state, source identity, pagination, duplicate handling,
retry/end status, genre encoding and ordering into Rust. Include the
TMDb/Trakt browsing requests used by these flows;
this does not complete their broader tracking/enrichment integrations. Retained
Details and collection services can consume complete Rust records until their
own rewrites. Translate each complete touched C UI component while preserving
its rendering, illustrations, layout and remote actions.

Only this rewrite's versioned private Rust JSON is eligible for restoration.
Scope history/results to the member, owner and selected profile; clear only this
capability's private files on logout. Do not add original-file readers or
compatibility import paths. Reuse the existing Rust HTTP, addon/manifest, title
ownership and guarded publication foundations rather than copying C parsers.

## Existing cases and planned Rust coverage

- Translate `tests/buscanorm.c`: case/accent matching, valid non-Latin text,
  invalid UTF-8, character counting, complete-character truncation/deletion.
  Preserve the distinction between local normalized matching and original
  provider query text.
- Translate `tests/buscasrec.c`: minimum length, trimming, ordering, duplicate
  promotion, ten-entry retention, restart, remove/clear, isolation and logout.
  Test Rust JSON instead of copying the old text-file fixture protocol.
- Inventory `tests/metaprov.c` search/fallback cases before replacing that path.
  Cover the Cinemeta preference and addon changes invalidating a repeated query.
- Add provider and native integration cases for late/failed replies, cancelled
  queries, profile switches, long configured URLs/IDs/queries, incomplete and
  empty payloads, incremental publication, title ownership and bounded workers.
- Add pagination cases for variable page sizes, raw versus accepted item counts,
  overlapping/repeated/empty pages, retry after failure, source/filter switching,
  generation cancellation, retained position, collection source tabs and ordering.
- Retain the visual/navigation assertions in `tests/busca_shot.c` and
  `tests/vertudo_shot.c` as C UI checks, adapting their data access to Rust.
  Live TV guide-search pagination fixtures are a separate backend area.

Run the relevant Rust, native, memory and Linux checks for the completed area,
push tested checkpoints, then build the isolated package before a manual handoff.

## Integrated text and recent-query checkpoint

The original People/name-search service and its network requests are removed
from Search and Spotlight. Cast metadata and the cast-to-filmography action in
Details remain. The other reviewed browsing features remain enabled.

Rust's UTF-8 decoder and the pinned `unicode-normalization` crate replace the
custom C decoding/folding table. Original provider query text remains distinct
from the local matching key. Latin accents and casing fold for local matching;
other scripts retain their characters. Invalid input bytes become spaces, and
native output copies stop at a complete character. The crate's
[upstream manifest](https://github.com/unicode-rs/unicode-normalization/blob/v0.1.24/Cargo.toml)
declares a Rust-only dependency path and a minimum Rust version below the TV's
Rust 1.85.1 toolchain; the final ARM build also verifies the selected lockfile.

`SearchHistoryRuntime` owns account/owner/selected-profile history and a single
serial storage worker. Rendering reads immutable native snapshots. A switch
immediately selects that profile's memory cache or an empty view while its Rust
JSON loads. Confirmed edits made during restoration merge in order, including
clear/remove. An outgoing profile's unsaved history remains in its own scope.
Logout cancels obsolete writes and queues deletion ahead of any new-session
loads. Normal shutdown drains confirmed edits. A damaged Rust history file is
retained for retry rather than silently overwritten; pending edits remain in
memory. No original-app files or discarded rewrite files are imported.

The recent-query C component has English names, comments and visible controls,
with its original chip geometry, animations and remote actions retained. The
rest of each Search screen will receive its complete component translation as
its query/results backend is integrated. The Rust store has no old 47-byte
query limit; the retained C query-entry fields still have their original display
limits and will be replaced during that integration. Native projections must
never cut a UTF-8 character when displaying/replaying a history query.

`tests/buscanorm.c` and `tests/buscasrec.c` are replaced by Rust text, history and
worker tests. Additional cases cover canonical Unicode deduplication, full
queries, private versioned JSON, account/owner/profile isolation, ignored old
files, corrupt envelopes, guarded writes, restoration races, edits during a
write, logout during a write and normal shutdown. Rust native drivers exercise
the real C Search/Spotlight builders and history actions. Retained screenshot
fixtures continue checking C navigation and rendering against Rust history.

The native catalog fixture now pumps the next C frame after asynchronous scene
restoration: a safely rejected superseded frame must not be mistaken for a
published view. This is a fixture timing correction, with production publication
guards unchanged.

### Focused manual checks

Run `bash tools/linux.sh` with the Rust Test account; use normal account mode
for history/profile checks. Search for a movie and a series, enter results,
open Details and return. Clear the query and replay a recent search; remove one
with hold-OK and clear the list. Restart to confirm history persistence. Repeat
in another profile and verify that histories stay separate.

Search a cast member's name in both Search and Spotlight: no People category
or person result should appear. Open a title's Details cast list and its actor's
filmography to verify the retained action. Title matches with the same name may
still appear as ordinary titles. The existing collection tabs, genre selection,
See All paging, retry and returning to the same position remain available.

## Verified package, 2026-10-17

Application source: `e44e99d36e175e49626629336a801885a5c086f7` on
`codex/rust-backend`. This is an intermediate Search integration, with the
remaining provider-search and expanded-browsing rewrite still pending.

- 234 Rust backend tests and 55 native integration cases pass.
- All 47 native catalog/title/capture/search cases pass under ASan/UBSan.
- Both retained C Search and Spotlight visual/navigation fixtures pass;
  screenshots are in `build/backend/search-visual/screens` (ignored artifacts).
- Retained C sync-order and offline-account regressions pass.
- Linux build, normal account-screen startup and clean shutdown pass with
  disposable test data.
- ARM Rust/C linking, soft-float ABI, glibc imports no newer than 2.12,
  exact package allowlist and executable permissions pass. Runtime/full/debug
  artifacts have the same build ID. TV runtime testing remains pending.

Package: `build/backend/webos/space.mkv.nuvio.rusttest_2.1.0_arm.ipk`.
Identity remains `space.mkv.nuvio.rusttest` / **Nuvio Rust Test**.
Build ID: `2fee45325947db8ca1ce2778cadcabf80171f414`.
IPK SHA-256: `433f5487a3ee9cefd5ddac5978f6c4034ebe99a6b54350e608d77bea5fe42fd3`.
The matching runtime, full binary, debug symbols, ELF report and `build.json`
remain beside it. The preceding verified package is preserved under
`build/backend/webos/checkpoints/package-before-e44e99d3`.

For Linux, run `bash tools/linux.sh` in normal account mode. Check the focused
search/history cases above. The same IPK is ready for a targeted TV check of
login, profile-specific Home catalogs, movie/series search, recent-search
persistence and remote actions. Confirm that Details cast/filmography still
works. The package has not been installed or launched on a TV automatically.
