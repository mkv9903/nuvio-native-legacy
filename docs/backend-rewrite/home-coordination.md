# Home loading coordination

The user approved keeping progressive first-load rows, stable cached Home during
background refresh, row limits and saved order, disabled rows, later candidates
when early catalogs are empty, collections, Continue Watching, saved-list inputs,
and immediate removal of disabled or removed addon rows. The user approved
removing automatic new-addon promotion: preserve saved order and append new rows.

## Implementation boundary

`backend/src/home_loading.rs` owns catalog enumeration, eligibility, ordering,
request scheduling, fallback and immutable catalog-scene assembly. It uses the
complete Rust addon manifests and configured URLs, not the previous C declaration
quotas or addon limit. Explicit local order precedes account order; saved declaration order
precedes new candidates. Unordered fresh candidates are interleaved by addon.
Required extras, hidden rows and catalogs represented by visible collections are
excluded. Channel rows still require explicit selection. New channels registered
as Settings suggestions do not enable themselves on a later refresh.

`CatalogRuntime::prepare_home` loads unknown enabled manifests with at most four
workers, using the existing Rust provider validation and private document cache.
Known useful manifests remain usable. A missing manifest can retain rows from
this rewrite's private scene only for a still-installed addon; an explicit new
manifest that removes a catalog cannot restore that catalog through this path.

Catalog loading uses at most three network workers and a bounded result channel.
Later eligible candidates fill slots left by empty or failed requests. Failed
loads retain a matching private cached row; confirmed empty results clear it.
Each cold-load result can publish an ordered partial scene. A populated cached
Home receives progress notifications until the complete replacement is ready.
Title records are shared immutable owners; assembling partial scenes does not
clone all their artwork, episodes or other metadata.

The C adapter projects the immutable result into the existing UI publication
path. Continue Watching, saved lists, tracking and collection services keep their
C implementations until their separate rewrites. Their scoped contributions and
row composition are retained. Local row-editor persistence, styles, layout and
immediate rearrangement also remain C; this step does not claim those services or
all their fixed UI projection limits are rewritten. No original C data is read
into a new Rust format. Existing Rust-private catalog/document/scene storage is
reused. A private versioned `home-plan-<scope-hash>.json` stores declaration order,
including unsuccessful and hidden catalogs. It is saved before requests start so
a failed preferred row is retried before its temporary replacement on the next
refresh or restart. It is scoped to member/owner/profile and removed with Rust
scene data on logout; no C file is consulted.

A plan is bound to its account, selected profile, addon selection and manifest
declarations. Profile changes or meaningful source changes cancel it before
publication. Registration in the retained row editor checks both the selected
profile and its activation generation under that editor's mutex. Switching away
and back cannot make an old worker valid again. The no-account path waits for
account readiness instead of continuously launching replacement Home workers.

The old C producer is reachable only in uninitialized pure-C reference fixtures.
The initialized application always uses the Rust coordinator; failure does not
fall back to that producer. Existing C services that use manifests outside Home
remain for their own rewrites. The obsolete C promotion implementation and its
assertions were removed; the chosen placement rule is tested in Rust.

## Test inventory

Fifteen Rust domain tests cover:

- Complete enumeration beyond the old addon/declaration quotas, local/account/
  accepted order, no new-addon promotion and duplicate suppression.
- Hidden rows and account aliases, disabled addons, visible versus hidden
  collections, required extras and explicit channel selection.
- Empty and failed catalog slots, later candidates, failure cache retention and
  authoritative empty results.
- Progressive cold loads despite delayed earlier replies; stable warm Home,
  including saved-list-only data; ordered final assembly.
- Three-worker concurrency, cancellation before and during publication, and
  release of shared title owners; failed preferred rows retain their retry position.
- Missing-manifest fallback and explicit catalog removal.

Eight new Rust-driven native tests execute the real choices/publication adapter:

- Complete configured URLs and title/catalog IDs beyond C display buffers.
- Continue Watching and saved-list offsets, flags and title-owner lifetimes.
- Rejection of a plan after manifest changes or reuse for another profile job.
- Delayed old-profile provider replies rejected after a profile switch.
- Row-editor registration rejected after switching profiles and returning.
- First-start manifest fetching/parsing before catalog loading.
- A manifest outage retaining this rewrite's private Home scene.
- Repeated registration of unselected channels never enabling those rows.

The Rust fixtures use fake account/provider transports. Retained C row-editor,
worker-slot, quota-reference, collection and progressive-reference checks cover
capabilities that remain C. Their legacy reference path is not evidence that the
production application fell back to C scheduling. ASan/UBSan checks cover the
real native projection, publication, capture and release paths; leak detection is
disabled for application process-lifetime globals, with Rust weak-owner checks
for transferred title lifetimes.

## Manual checks

Use normal account mode, not the isolated screenshot preview:

```sh
git switch codex/rust-backend
git pull --ff-only
NUVIO_LOG=/tmp/space.mkv.nuvio.rusttest.log bash tools/linux.sh
```

Check cold startup and a warm restart; saved row order and disabled rows;
independent and primary-sharing profile switches, including selecting the
secondary profile first; collection rows and Continue Watching; and refreshing
with a temporarily unavailable provider. A newly installed addon should append
behind saved rows and may stay outside a limited Home until its order is changed
in Settings. Existing saved-list and playback navigation should behave as before.

Linux exercises the UI and account/provider data flow. LG native playback and
actual old-TV timing still require the isolated TV package; local tests do not
establish TV runtime performance.

## Validation record

The application passes 214 Rust backend tests, 53 Rust-driven native integration
tests and 45 ASan/UBSan native catalog/title/capture cases. Linux compilation and
a ten-second disposable normal-mode startup/clean shutdown pass. The login-screen
startup launches one Home worker, with no retry loop. Rust formatting and diff
whitespace checks pass.

The retained row-editor, quota-reference, worker-start race and current real
collection/Home fixtures pass. The reused worker fixture needed its manifest
callback updated to the already-existing integer-returning interface; the
uninitialized-runtime helper now supplies unreachable registry inputs used by the
new adapter. These are fixture/link changes, not application fallbacks.

The older `tests/colfileiras.sh` discovery fixture is not counted as passing: it
already defines the obsolete void-returning manifest callback and lacks several
current catalog/artwork helpers. Its existing collection eligibility cases are
covered by the Rust planner and the current `tests/colfileiras_home.sh` real
Home/editor integration. Metadata/name-lookup reference assertions in that old
fixture remain a test-maintenance follow-up for their service rewrites. This
limitation does not apply to the full-source native Rust driver suite.

Four additional Rust cases verify preferred-row retry after replacement, private
plan-key persistence with full values and owner/profile isolation, cancellation
preserving the prior file, malformed documents and selective namespace cleanup.
The native complete-value case also asserts that the actual runtime saves the
full declaration key before requesting a catalog. Saved declaration order is
separate from the visible scene; a successful fallback cannot permanently
consume a preferred failed row's retry slot.
