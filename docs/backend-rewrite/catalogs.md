# Addons and Home catalogs

## Approved scope and behavior

Rust will own account addon loading and edit persistence, manifests and catalog
loading, remote Home catalog order and account/profile isolation. The retained C
UI must keep its appearance and navigation. Source and subtitle search algorithms
and general settings remain in C for their later rewrites; their data access may
consume stable Rust-owned snapshots.

User-confirmed decisions:

- On a profile change, show only the target profile's cached catalogs or the
  existing loading placeholders until its rows arrive.
- A successful empty addon list is authoritative. Clear that profile's addon
  rows; retain its last valid cache only when loading fails.
- Reject an entire malformed addon-list response and retain its valid cache.
- A confirmed empty catalog (`metas: []`) clears its old titles. Failed or
  malformed replies retain only that context's valid cache.
- Skip unusable individual catalog entries, such as entries without a name or
  poster, and show valid titles from the same response.
- Follow Stremio's independent object-resource ID filters. An omitted object
  `idPrefixes` accepts all IDs; string resources inherit root filters.
- Remove the 32-addon and 2,047-character URL limits from Rust domain models
  and adapt the remaining C consumers. Bound network work and response bytes
  separately to fit the TV; never silently truncate a provider URL.
- Read only this rewrite's versioned Rust JSON. Do not import old C caches.
- Preserve unreadable pending edits and pause synchronization for that scope.
  Display a valid same-scope cache, but block further edits and uploads while
  recovery is pending. Retry transient read failures; never silently delete or
  overwrite the journal. A damaged cache alone can be replaced by a valid fetch.
- Merge pending TV intentions into a newly fetched server list before every
  upload. TV enabled/disabled changes win for matching provider URLs. Remote
  additions and ordering remain; remote removal wins over a TV toggle. An
  explicit TV install can reinstall a remotely removed provider and appends it.
- Use fetch-then-merge with the existing whole-list RPC. The user accepted that
  another device can still change the list between that fetch and upload. The
  server has no atomic merge operation available in this repository, so this
  does not guarantee conflict-free simultaneous writes.

## Existing contracts to preserve

Account addons come from the authenticated `addons` table, filtered by confirmed
account owner and effective addon profile, ordered by `sort_order`. There is no
working `sync_pull_addons` RPC. Shared profiles that inherit primary addons use
the primary profile's addon list while keeping their own Home order and private content.
Local edits survive failed pushes. An acknowledgement must match the account,
profile, generation and exact edit revision; a late reply cannot erase a newer
edit or publish into another profile.

Home order is read-only account configuration. Applying an incomplete TV catalog
as an upstream replacement could remove rows on other devices. Remote ordering
is a union: known local rows follow remote order, then unknown local rows retain
their previous order. Hidden short identities and long provider keys both work.
Missing configuration preserves state; an explicit empty configuration clears
remote order/hidden choices. Local TV choices retain their existing precedence.

Manifest parsing must use root fields, support string and object resources,
resource type/prefix filters, required catalog extras, channel types, and search
catalogs outside the Home display quota. Manifest and catalog results carry
their originating context; stale work cannot publish or overwrite another
profile's cache. External addon requests must never receive Nuvio authorization
or public-key headers.

Protocol reference: [Stremio manifest specification](https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/api/responses/manifest.md).

## Reference checks

Before this area changes, `tests/catordem.sh` and `tests/catordemcache.sh` pass
at `f8b4f72a`. Their cases cover modern and legacy order, collections, sorting,
unknown local rows, hidden keys, repeated replies, missing versus empty replies,
optional booleans, large accounts, logout, concurrent publication, restart,
profile/account ownership and cache removal. These backend cases move to Rust.

## Progress

Home-order parsing, sorting, hidden choices, union and versioned cache now run
in Rust through the retained C sync/rendering interfaces. Their backend tests
are Rust tests; the C implementations and their C test programs were removed.
Rust addon/manifest models are implemented and tested, including long URLs,
large lists, resource filters and catalogs outside the Home quota.

Context-bound account addon networking, private cache/pending-edit storage and
immutable native addon snapshots are implemented and tested. Requests use the
confirmed owner and effective addon profile, use Serde for pushes, paginate
large lists, reject incomplete multi-page results, and reject old work after a
profile round trip. Saved edits carry unique revision IDs. Acknowledgement
promotes the confirmed merged list into cache before removing its pending
record. Independent store handles use advisory file locks to protect read/check/removal.

The C account/profile UI regression driver still passes. Native snapshot tests
cover large lists, complete URLs, resource filters, catalogs outside the Home
quota, shared readers, parent release and separately owned request URLs.
`ring` is a direct dependency for SHA-256 scope filenames; its exact version
was already present in the pinned TLS dependency graph. `uuid` adds its Serde
feature for persisted edit IDs. No additional package versions were introduced.

The addon backend and one bounded worker now implement the confirmed edit
policies. Actions modify memory and enqueue I/O. Idempotent install, removal and
set-enabled intentions use a version-2 JSON journal rather than a copied addon
list. Cache JSON remains version 1. These prepared addon journals have not been
used by the application; no legacy journal is imported or converted.

Compare-and-replace writes and exact-ID acknowledgements preserve newer edits.
An action submitted during an upload stays pending after that upload completes.
Edits accepted before a profile change are still saved locally, even when their
network job is cancelled. A disk failure retains them in memory and reports an
unsaved state; they cannot survive an app/process crash until a write succeeds.
Network failures never upload the cached list. Conflicts with another local
process pause the scope instead of overwriting that process's journal.

Frame-boundary context selection immediately uses the target scope's in-memory
cache or an empty loading snapshot. Workers restore its disk cache before
network work. Shared-primary addon scope is distinct from the selected UI
profile and its request generation. Metadata publications have separate data
and list revisions, so capability updates do not invalidate an unchanged UI
list. Native projections share complete immutable strings and arrays, including
status/error fields; native actions validate the originating list and context.

The worker retries failures after 30 seconds and refreshes valid lists after
five minutes while the app supplies frame steps. Only the latest queued profile
request is retained. Drop detaches during an in-flight request and flushes local
edits; logout hides state immediately and queues private addon-file cleanup.

Manifest/catalog HTTP and cache integration, C
source/subtitle snapshot access and profile-change publication still need
implementation. The application still uses the existing C addon
loading/search data until that integration is complete. This checkpoint does
not yet fix the profile-switch Home-row issue or remove the application's old
addon count/URL limits. The new Rust models have no such fixed-buffer limits.

Implementation is in progress. This document is not a Linux or TV test handoff.
The first account flow has user-confirmed Fedora QR and email/password login;
the new Rust runtime has not yet been verified on the TV.
