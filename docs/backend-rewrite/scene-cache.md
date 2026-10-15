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

`SceneSnapshot` now restores this JSON into immutable Rust title and row owners.
Rows retain their complete keys, configured provider URLs, catalog IDs, encoded
extra requests and explicit loading/empty states. Retained title or row readers
outlive the scene without leaking its other records. Native title records keep
all artwork variants, cast/person IDs, commercial providers, display metadata,
resume fields, seasons and membership flags. Native text projections sanitize
display NULs and stop at UTF-8 boundaries. Full request identities and complete
arrays remain in Rust; the transitional C display arrays retain their existing
quotas until their consumers adapt.

Six additional Rust tests verify complete native restoration and final-owner
release, configured/encoded row requests, distinct empty/loading state, invalid
windows, safe out-of-range queries, UTF-8 projection, cancellation after native
construction and rejection of original/discarded storage and foreign app roots.
An additional Rust-driven C test verifies the actual restored title projections,
including cast/season display quotas, retained complete data and final release.
The checkpoint passes 184 backend tests and 21 native integration tests.

Storage is private to this new Rust Test app. There are no legacy readers,
converters, original-app directory searches or compatibility fallbacks in the
Rust scene restore path. Only versioned Rust JSON belonging to this rewrite is
eligible for restoration.

The integrated catalog runtime now connects this store to the C application's
assembled Home scene. Live capture, asynchronous restore and scoped publication
use retained Rust titles and rows. The original binary reader/writer,
compression and format-compatibility tests have been removed; their replacement
assertions are Rust tests. No original-file fallback exists. See
[catalog runtime testing](catalog-runtime-testing.md) for the current test build
and manual checks. The counts above describe the earlier restoration checkpoint.

The build-only checkpoint at `c7198054` also passes webOS ARM compilation,
soft-float ABI, imported glibc maximum 2.12 and matching executable/debug build
IDs. Its artifacts and hashes are archived separately. The existing canonical
IPK and matching runtime/symbol files were restored and verified byte-for-byte;
this compatibility build was not installed or tested on the TV.
