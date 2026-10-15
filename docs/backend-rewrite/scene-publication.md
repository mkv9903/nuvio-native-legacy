# Scene capture and scoped publication

The transitional capture adapter reads live, retained C display records into
the Rust scene model. It does not read C cache files or reproduce their binary
layout or compression. Rust validates the complete result and Serde handles
its private JSON representation.

Unchanged projections keep complete provider identity, independent IMDb hints,
ratings before rounding, artwork, formatted display fields, commercial
availability, resume values, cast/person IDs and seasons. Existing Rust records
remain the source of values that C cannot represent. When a C writer changes a
full display-array projection, its prefix updates the complete vector while
entries beyond that projection remain retained. A smaller count or explicit
zero count replaces or clears the vector. Malformed text/counts/records reject
the whole capture; no partially captured scene replaces a valid cache.

`SceneBackend` binds work to the verified selected profile and installed-addon
configuration. Shared primary addons do not share private Home scenes. Its
frame activation reads memory only: a profile switch publishes that profile's
cached scene or no scene. Separate restore/publish tokens reject previous
accounts/profiles, A-to-B-to-A replies, superseded requests, old file restores
after fresh data, and old writes after a newer scene has been accepted.

Visible catalog rows must still belong to configured enabled addons. Unknown
manifests retain useful rows; explicit removed catalog declarations and missing
required request extras exclude them. Removed row-exclusive titles are pruned
and retained row windows are rebuilt. Continue Watching/other synthetic rows,
saved-list membership, collection membership and resume metadata stay available
for this profile. An explicit empty scene replaces its old data.

File errors preserve fresh display data and the last valid file. Status-only
updates reuse the visible scene. Clearing invalidates old work before deleting
only the Rust scene files, and blocks new restoration while cleanup is active.
The persistence layer continues to check work immediately before atomic rename;
these checks do not hold account locks across filesystem operations, so callers
must also validate a snapshot at publication.

Six new Rust-driven native cases cover actual C capture round trips through
private JSON; long values and array tails; scalar/rating fidelity; live
enrichment/progress changes; explicit removals; malformed records and cache
retention; live records without a Rust source; display NULs and values outside
C scalar ranges. Eleven Rust service cases cover profile/cache isolation,
generation/configuration guards, delayed restoration, superseded writes,
authoritative empty lists, manifest changes, counter-only updates, storage
failure, blocked unverified contexts, clear races and row-window rebuilding.
The full checkpoint passes 195 backend tests and 27 native integration tests.

The implementation checkpoint `839e1b17` also passes Linux compilation and
disposable preview startup, plus webOS ARM compilation with the soft-float ABI,
imported glibc versions no newer than 2.12, and matching runtime/full/debug build
IDs. Build-only artifacts and hashes are archived under
`build/backend/webos/checkpoints/839e1b17`. No IPK was created and no TV runtime
test was performed. The earlier canonical package/runtime/symbol files were
restored and hash-verified after the build.

This checkpoint supplies capture and publication services. It does **not yet
connect them to the application's Home publication/cache path**. Remaining
native reader/row ownership and runtime wiring must be complete before replacing
the binary reader/writer and enabling Rust catalog production. The existing
profile-switch row bug is therefore still pending. No TV handoff is requested.

The remaining integration order is:

1. Retain catalog records under the publication lock before metadata workers
   leave for network requests. Their current raw display copies are insufficient
   once the source record has a Rust owner.
2. Finish ownership transfers in search results, expanded catalog grids and
   saved-list consumers. Their returned records must remain valid after a result
   reset, profile change or Home replacement, and release on every exit.
3. Retain complete row records through C Home assembly, replacing truncated row
   keys/request fields at consumers without changing the visible layout.
4. Wire the scene service into account/profile activation, asynchronous restore,
   provider loading, frame publication and capture of retained C enrichment.
   Publish the target profile's cache/loading state before starting its requests;
   validate work again at every UI and persistence boundary.
5. Remove the old binary scene reader/writer and its compression/version rules
   when the runtime switches to this private Rust JSON store. Do not add a file
   importer or fallback reader. Run integrated profile-switch, delayed-response,
   empty-result, restart and native-lifetime checks before a manual TV handoff.
