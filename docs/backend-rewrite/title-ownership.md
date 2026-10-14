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

Raw C binary cache writing rejects native-owned records; native pointers must
never be persisted. Binary reads clear the pointer field before publication.
This is an interim guard, not the new Rust catalog cache. The loader already
has versioned provider-document JSON, and a complete scene model/private JSON
store is now tested in Rust. [Scene cache](scene-cache.md) still needs native
capture/restore and application wiring before native titles are published.

## Remaining integration

This checkpoint does **not** enable the Rust title producer in the application.
Catalog loading still uses the C producer. Before switching it over, adapt
worker arrays and remaining temporary copies;
replace the assembled binary cache; and connect complete IDs and image strings
through Details, source selection and rendering consumers. Existing enrichment,
episode, progress and commercial-availability data must survive those changes.
Profile-scoped catalog scheduling and publication remain part of this milestone.
The previous-profile Home-row bug is not yet resolved.

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
readers and rating scaling. Seven Rust-driven native tests use the actual C
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

No TV handoff is requested for this foundation checkpoint.
