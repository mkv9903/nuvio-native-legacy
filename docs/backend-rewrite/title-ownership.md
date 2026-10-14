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
This is an interim guard, not the new Rust catalog cache. The new loader already
has versioned provider-document JSON; the assembled scene cache must also move
to Rust before native titles are published in the application.

## Remaining integration

This checkpoint does **not** enable the Rust title producer in the application.
Catalog loading still uses the C producer. Before switching it over, adapt
long-lived UI selections, deferred actions, worker arrays and temporary copies;
replace the assembled binary cache; and connect complete IDs and image strings
through Details, source selection and rendering consumers. Existing enrichment,
episode, progress and commercial-availability data must survive those changes.
Profile-scoped catalog scheduling and publication remain part of this milestone.
The previous-profile Home-row bug is not yet resolved.

## Verification

Five Rust title tests cover complete strings, provider/IMDb identity, independent
references, Unicode projection, malformed inputs, display NULs, concurrent
readers and rating scaling. Three Rust-driven native tests use the actual C
catalog to cover held selections across replacement, final-owner release,
legacy artwork enrichment and frame-safe in-place updates. Remaining C catalog
tests continue to exercise their unmigrated algorithms; they are not replacements
for the Rust catalog-loader tests.

No TV handoff is requested for this foundation checkpoint.
