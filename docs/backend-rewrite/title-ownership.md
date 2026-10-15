# Title ownership and catalog integration

The user approved retaining all existing title/catalog features: addon rows,
movie/series/channel records, search and genre filters, pagination, artwork and
fallbacks, synopsis/year/runtime/ratings/cast, episode and resume data, and the
Details commercial-availability fields. Enrichment services will have their own
feature review when rewritten. This work changes data access and lifetime,
preserving the original C screens and navigation.

The user also approved correcting the old rating conversion: 8.5/10 remains
85%, while 10/10 and scores rounding to 100% remain 100%. The C conversion's
second scale adjustment incorrectly turned those scores into 10%.

The retained C catalog reader, detail enrichment and partial metadata updates
now call the same Rust conversion helper. The correction therefore runs in the
current application as well as the new Rust title model. Invalid/nonfinite
scores have no displayed rating; missing scores in a partial detail response
continue to preserve the existing score. The full C metadata reader remains
until its loader is replaced.

## Native title foundation

Rust owns complete provider IDs, separate IMDb hints, title strings and artwork
URLs in immutable, reference-counted snapshots. Native readers retain a snapshot
when copying a title. Text is borrowed from that live snapshot; releasing one
reader does not invalidate another. Provider identity is immutable and is never
replaced with an IMDb hint.

The retained C record currently projects text into its existing fields for
unmigrated writers. A new accessor returns the complete Rust value when that
projection is unchanged, or the C enrichment overlay when it has changed.
Projection stops at a UTF-8 boundary. Those display buffers do not limit the
Rust domain. Unmigrated enrichment fields still have their existing limits.

Catalog publication, append, merge, row copy and in-place update now retain
native owners. Replaced catalog blocks release their owners during the existing
frame retirement. In-place updates retire their previous native reference for
two frame advances so an already borrowed text pointer remains valid while
that frame is rendered. Held selections require their own references.

Home catalog loading now publishes Rust-owned titles and rows. Private provider
documents and assembled scenes use versioned Rust JSON. The original C binary
cache and its compatibility rules have been removed, so native addresses cannot
be persisted by that path. See [scene cache](scene-cache.md).

## Remaining integration

The integrated runtime uses complete Rust manifest catalog requests, owns worker
results and rows, and applies the selected profile's cached scene or loading
state at a frame boundary. Scoped publication rejects old profile replies;
metadata and Continue Watching callbacks also carry verified context leases.
Home focus and Details transitions retain their chosen artwork. Automated
profile-switch cases pass; real-account confirmation is part of the manual
handoff in [catalog runtime testing](catalog-runtime-testing.md).

C Home orchestration, metadata enrichment, expanded catalogs, search and other
services remain for their own rewrites. Their remaining interfaces and display
quotas do not impose limits on the Rust domain, but not every unmigrated consumer
uses the complete values yet. Existing UI layout and native playback remain.

Details, playback, live-channel and saved/social menu selections now own their
references. Details re-resolves the complete held provider ID and retains its
record through the closing animation. Playback keeps its record during native
retention and releases it only after actual session teardown. The router's
next-episode wait releases its title on cancellation, timeout or consumption.
Deferred save/watched actions use explicit clone/release callbacks; execution,
replacement and shutdown release their references. Legacy plain-data notice
contexts can still be copied by value.

The touched action/save notification components are translated into English
files, names, comments, logs and visible text. Their island renderer and layout
are retained. Small English-commented compatibility headers keep unmigrated
callers working. Existing C settings marker filenames remain part of the
unmigrated settings backend; they are not Rust save-file imports.

## Verification

Five Rust title tests cover complete strings, provider/IMDb identity, independent
references, Unicode projection, malformed inputs, display NULs, concurrent
readers and rating scaling. Thirteen Rust-driven native tests use the actual C
catalog to cover held selections across replacement, final-owner release,
legacy artwork enrichment and frame-safe in-place updates. Remaining C catalog
tests continue to exercise their unmigrated algorithms; they are not replacements
for the Rust catalog-loader tests.

The additional native cases exercise held Details/playback selections and
re-insertion by complete ID, the actual saved-menu/channel entry points,
one-time undo execution after its source is released, replacement/cancellation,
final-owner release and context-clone failure. The retained C playback-retention
fixture also passes, including pause confirmation, same-session resume,
timeouts, account/profile changes, backend replacement and saved-source behavior.

The native rating case exercises the actual C catalog/detail/partial-update
paths with decimal and percent variants, numeric/string perfect scores, near-10
rounding, tiny positive values, missing values, NaN/overflow and invalid scores.

Those counts describe the foundation checkpoint; the current integrated build
passes 196 backend tests and 45 native tests.

## Home assembly ownership checkpoint

The retained Home assembler now owns its catalog task results, cached-row
copies, accumulated title batch and saved-list batches explicitly. The last
round participant releases ready and late results even when the assembler has
cancelled. Publication retains its own references before temporary saved-list
tails are released; later catalog appends cannot overwrite an unreleased tail.
Final, cancelled and superseded assembly exits release the accumulated batch.

Continue Watching's temporary inputs, enriched copies and publication copies
also release their references. Enrichment/progress filtering releases rejected
titles and transfers surviving records when compacting. Optimistic composition
still uses a short-lived borrowed output: its retained inputs live through
publication and are released afterwards. Its ordering/filtering behavior remains
C until that capability's own rewrite.

Four additional Rust native-driver cases exercise actual shared-round cleanup
in both participant completion orders, publication followed by cancellation and
a late worker, forty partial saved-list publications, and enrichment/progress
compaction with final-owner checks. The full suite passes 178 backend tests and
20 native tests. Linux builds and starts using disposable Xvfb preview data.
Retained local Continue Watching checks also pass with ASan/UBSan; retained
ordering, duplicate, Trakt contract/startup and watched-action checks pass.

These checks exposed existing fixture drift: the no-addon doubles had the old
void manifest-result signature and omitted newer discovery functions; the date
fixture expected Portuguese despite the existing English-only month formatter;
the Trakt duplicate fixture lacked function/data sections for its selective
linking. Test-only updates restore these fixtures without changing their
behavioral assertions (except the required English date text). The unchanged
pre-checkpoint discovery/Trakt sources reproduce the original fixture failures.
Replace these C fixtures with Rust cases when their capabilities are rewritten.

At that assembly checkpoint, native scene capture and runtime Rust catalog
publication were still pending. The subsequent integrated runtime completes
those paths. Search/grid and other C services still require their own rewrites.

The subsequent native scene restoration checkpoint makes `TitleSnapshot` own
the complete `SceneTitle` directly rather than duplicate its metadata model.
The existing C projection includes artwork alternatives, logo language evidence,
classification/director, all commercial providers, resume/membership fields and
cast/seasons. The full Rust cast/season vectors and complete strings remain
available independently of the transitional C display quotas. Six Rust scene
snapshot tests and one additional actual-C projection case cover this restore
path; total checks are 184 backend tests and 21 native tests.
