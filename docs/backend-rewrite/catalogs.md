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
- Limit retained subtitle fetching to four concurrent requests, independently
  of provider count and the sheet's twelve-result display quota.
- Read only this rewrite's versioned Rust JSON. Do not import old C caches.
- Preserve unreadable pending edits and pause synchronization for that scope.
  Display a valid same-scope cache and block edits and uploads while recovery is pending.
  Retry transient read failures; never silently delete or
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
list. Cache JSON remains version 1. The application now uses these Rust journals; no legacy journal is imported or
converted.

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

The user also confirmed blocking further addon changes while an unreadable
journal awaits recovery. The backend and real C addon controls enforce that rule. The native UI driver
verifies that cached addons remain visible, remote Enter cannot toggle them and
the damaged journal is unchanged.

Catalog title parsing now retains complete IDs, artwork URLs and other strings.
The user confirmed adapting C consumers to those complete values, including
related details, playback and cache data access, while keeping their UI design.
If both a provider ID and an IMDb hint exist, provider requests use the provider
ID; the IMDb value remains separate for artwork and tracking. A nonempty catalog
with no usable entries is a failed load, retaining its valid cache. An explicit
empty list remains authoritative. Mixed lists show their usable entries.

Provider HTTP is isolated from account HTTP and has no account header inputs.
Configured query/path values survive complete encoded resource URLs. Redirects
are bounded, bodies are limited to 4 MiB, and catalog/manifest requests have
8/12-second deadlines. Versioned original JSON documents are stored privately
under hashed member/owner/selected-profile/provider/resource/extra keys. No C
binary catalogs are imported. Cache writes reject invalid and cancelled replies;
network failures can use only that request's own valid cache.

The loader guards account/profile context before/after disk, network and decode
work. A profile round trip cancels older requests before publication/cache
replacement. A corrupt cache does not block a fresh valid reply. A valid network
reply remains usable when caching fails, with a separate storage error.

The pinned `serde_json` adds its `raw_value` feature, with no package-version
changes. Catalog entries are borrowed as raw JSON and decoded one at a time;
only the requested renderer quota is materialized. Full domain field lengths
are independent of that quota. The entire JSON syntax is checked, including
entries beyond the display quota.

The user delegated incomplete-manifest handling to standard Stremio behavior
and the recommended cache-preserving rule. Replies omitting both resources and
catalogs, null declarations and non-null error replies fail validation. They
cannot replace a useful cached manifest. Explicit empty declarations clear old
catalogs, and source-only manifests can omit catalogs. Legacy catalog-only
providers remain supported. Full object resources use their own type filters;
omitted object types do not inherit root types or authorize requests.

References checked on 2026-10-12:
- [Stremio manifest protocol](https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/api/responses/manifest.md)
- [Stremio client manifest implementation](https://github.com/Stremio/stremio-core/blob/master/src/types/addon/manifest.rs)
- [Nuvio self-host architecture](https://github.com/NuvioMedia/self-host/blob/main/docs/architecture.md)

The protocol lists resources/types as required. The client defaults omitted
catalogs to an empty list, allowing source-only addons. This rewrite keeps
existing provider compatibility for catalog-only declarations and does not
start requiring unrelated descriptive/version fields. Nuvio self-host defines
account/profile/addon synchronization, rather than provider manifest semantics.

## Runtime addon integration checkpoint

The app creates the Rust addon worker alongside the Rust account capability.
Frame steps activate the verified content context and refresh the C frame view;
selecting a profile activates its addon scope immediately. Automatic periodic
refresh remains suspended during playback, preserving the previous policy.
Accepted edit actions can still run, and logout hides addon state immediately.

The C addon registry, custom registry/manifest parsing, C file imports, addon
cache/journal and duplicate account read/upload paths have been removed. Small
English C adapters expose immutable Rust-owned strings and arrays to consumers
awaiting their own rewrites. The source and subtitle algorithms remain in C.
Each request job and its retry workers retain a Rust snapshot for their entire
lifetime. New context/list revisions cancel stale requests and prevent late
publication, while old borrowed strings remain valid for their held readers.

Source request URLs and subtitle provider request URLs come from Rust without
the former provider-count/base/request buffer limits. Configured path/query
values survive; title IDs and subtitle extras are encoded once. The retained
subtitle helper's already-encoded extras are decoded with UTF-8 validation before
Rust builds its request. Provider URLs stay out of the addon settings inspector;
it displays only the parsed public host and port, excluding credentials, path
and query. Source health counters belong to exact provider URLs and scopes.

Four subtitle workers share a provider queue. Every enabled provider can be
visited, even when the installed count exceeds 32; a busy set of four keeps the
fifth request queued. Profile changes cancel queued work and hide old subtitle
results. The existing language ordering, fairness, episode checks, subtitle
extras/fallback, range hash and sheet limits are retained for their later rewrite.

The complete touched addon screen controller and Settings addon renderer now use
English names, comments and text, with their original geometry, styling and
remote navigation. Guide addon action handlers also use English and reject
blocked edits. The recovery notice occupies the existing subtitle text area.
Screenshots and real remote actions are checked in the native Rust UI driver.

The transitional C manifest cache also validates replies with Rust before
replacement. A malformed or incomplete reply cannot evict a useful cached body.
Same-key successful saves replace their earlier body rather than leaving duplicate
entries. This cache and the remaining C catalog parsing will be removed when
Rust catalog orchestration is connected.

The remaining integration includes complete C title ownership/access, details
and playback title ID access, artwork request/cache access, manifest/catalog
orchestration and immediate target-profile Home publication. Discovery, guide
and title consumers still have legacy buffers/quotas pending that work. The new
Rust models have no fixed field/count limits; request/cache byte budgets remain
explicit for TV resources. This checkpoint does not yet fix the Home-row
profile-switch issue or claim that all title/guide/catalog URL limits are gone.

Implementation is in progress. This document is not a Linux or TV test handoff.
The first account flow has user-confirmed Fedora QR and email/password login;
the new Rust runtime has not yet been verified on the TV.
