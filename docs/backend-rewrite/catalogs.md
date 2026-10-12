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

## Existing contracts to preserve

Account addons come from the authenticated `addons` table, filtered by confirmed
account owner and effective addon profile, ordered by `sort_order`. There is no
working `sync_pull_addons` RPC. Shared profiles that inherit primary addons use
profile 1's addon list while keeping their own Home order and private content.
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
promotes the edit into cache before removing its pending record. Independent
store handles use advisory file locks to protect read/check/removal.

The C account/profile UI regression driver still passes. Native snapshot tests
cover large lists, complete URLs, resource filters, catalogs outside the Home
quota, shared readers, parent release and separately owned request URLs.
`ring` is a direct dependency for SHA-256 scope filenames; its exact version
was already present in the pinned TLS dependency graph. `uuid` adds its Serde
feature for persisted edit IDs. No additional package versions were introduced.

Pending user decisions before binding addon actions and synchronization:

- Pause on an unreadable/corrupt pending-edit file, or discard that edit and
  reload from the server. No recovery policy has been selected or bound.
- Merge TV edits into the latest server list, or preserve C's whole-list
  replacement. Merging changes the pending-edit model and acknowledgement data;
  do not wire the snapshot replacement primitive as the final policy yet.

The application controller, manifest/catalog HTTP and cache integration, C
source/subtitle snapshot access and profile-change publication still need
implementation. The application still uses the existing C addon
loading/search data until that integration is complete. This checkpoint does
not yet fix the profile-switch Home-row issue or remove the application's old
addon count/URL limits. The new Rust models have no such fixed-buffer limits.

Implementation is in progress. This document is not a Linux or TV test handoff.
The first account flow has user-confirmed Fedora QR and email/password login;
the new Rust runtime has not yet been verified on the TV.
