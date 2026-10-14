# Rust Home scene cache

`catalog_scene.rs` defines the complete publication independently of C record
layouts: title metadata and provider/IMDb identity, artwork variants and logo
language evidence, formatted display metadata, cast and person IDs, subscription/
rental/purchase providers, resume and episode information, seasons, membership
flags, row identities/windows, provider configuration and catalog extras.
Strings and arrays use Rust collections. C field/count limits are not domain
limits. Loading and confirmed-empty row states remain distinct.

`SceneCache` serializes this model using Serde. Version-one JSON envelopes carry
the test app ID, English language marker and selected-profile ownership including
the signed-in member and confirmed account owner. SHA-256 filenames do not expose
those IDs or configured provider URLs. The existing private Rust store supplies
0600 atomic files and advisory locks. No C binaries, struct sizes, compression
streams, native addresses or old-file imports are part of this format.

The complete encoded file has a 32 MiB budget. Serialization stops at the budget
instead of creating an unbounded intermediate JSON buffer. This is an aggregate
cache-byte limit, not a title-string or collection-length limit. Exceeding it
returns an explicit cache error and leaves the previous valid file untouched;
callers can still display fresh network data. Reads are bounded as well. Scene
windows and identities are validated before saving and after restoring; invalid
windows are rejected rather than redirected into another row's titles.

Workers supply their captured-context predicate. Restoration checks it around
I/O and decoding, including failed decoding. Saves check before encoding, under
the file lock, and after completing the temporary file immediately before its
rename. Cancellation cleans the temporary and preserves the previous scene.
These checks do not hold the account lock across filesystem work; the runtime
must check its context again before publication. Empty scenes and explicit empty
rows are valid authoritative saves. Damaged caches remain recoverable through a
fresh valid fetch; clearing removes only this cache's files and preserves addon
edit journals and other capabilities' data.

Fourteen Rust tests cover complete round trips; values and array lengths beyond
old C limits; perfect ratings and independent provider/IMDb IDs; member/owner/
selected-profile isolation despite primary-addon sharing; empty replacement;
invalid windows/records/requests; schema/app/language/scope checks; damaged data;
late reads/errors and cancellation after temporary-file creation; bounded reads/
writes; concurrent independent handles; private permissions, hashed names and
redacted diagnostics; symlink-safe recovery; legacy exclusion and selective
cleanup.

This is the tested model/storage layer. It is **not yet connected** to the C
application's assembled-scene cache. The legacy binary reader/writer remains
until worker/title/row ownership and native rehydration are ready. Its C tests
still cover its current runtime behavior. Connecting this Rust store and removing
those binary-cache routines is part of the active catalog milestone, followed
by context-scoped catalog scheduling/publication. No manual TV/Linux handoff is
requested for this checkpoint.
