# Core migration order and test policy

The next recommended batch is **text cleanup and search normalization**:
`dobra.c`, `limpa.c` and `buscanorm.c`. They total 455 lines in the current C
source, have existing focused tests and require no network, storage, SDL or GL.
They provide useful foundations for the later source/metadata flow without
introducing another Rust dependency.

The current C baseline passes `tests/dobra.sh`, `tests/buscanorm.sh` and
`tests/limpa.sh` (the latter with `ASAN_OPTIONS=detect_leaks=0` for the sandbox).
Those results establish the starting behavior; this text group has not yet
been ported to Rust.

The following inventory was inspected on October 11, 2026, after the add-on
URL port. Counts include comments and blank lines in the named `.c` files;
headers, included `.inc` files, fixtures and dependencies are additional. They
measure current source size, not rewritten Rust size or implementation effort.
Complexity also reflects coupling, mutable state, ABI ownership and device
behavior. This is a recommended sequence, not one large rewrite per table row.

| Order | Module or related group | Current C size | Complexity and scope |
| --- | --- | --- | --- |
| 1 | Text cleanup/search: `dobra`, `limpa`, `buscanorm` | 66 + 277 + 112 = **455** | Low to medium: Unicode/buffer edges; good next batch. |
| 2 | JSON reading/writing: `js`, `jsw` | 347 + 149 = **496** | Medium, broad reach: preserve tolerant reading, escaped Unicode, numeric behavior and writer ownership. Migrate the reader and writer as separate substeps in one milestone. |
| 3 | Stream models/parsing/filtering: `stream_parse`, `naovideo`, badge detection | **425** plus extracted logic from the 391-line `badges` module | Medium: nested fields, exact byte sizes, proxy headers, torrent IDs and returned-record ownership. Retain badge drawing in C. |
| 4 | Audio codec/channel labels: `audioinfo` | **64** | Low: pure formatting/tables; can accompany stream metadata work. Does not replace the native audio pipeline. |
| 5 | Catalog/Continue Watching ordering: `catordem`, `catordemcache`, `cwordem` | 385 + 93 + 190 = **668** | Medium: ties, timestamps, caches and shared state. Group the ordering rules; validate storage changes separately. |
| 6 | Storage foundations: `data`, `cachedisco` | 179 + 76 = **255** | Medium despite the small size: existing filenames, app isolation, write failures and file replacement behavior must remain compatible. |
| 7 | Source ranking/preferences: `fonteauto`, `fontepref`, `fonteregra` | 138 + 521 + 661 = **1,320** | Medium to high: start with pure queue decisions, then saved preferences and configurable/regex rules. Do not replace them all in one unverified step. |
| 8 | Source cache/statistics: `fontecache`, `addonstats` | 415 + 206 = **621** | Medium to high: bounded cache entries, profile/title generations, concurrent access and persistence. |
| 9 | Profile/watch state: `profile_content`, `progress`, `vistoep` | 154 + 475 + 305 = **934** | Medium to high: preserve ownership, profile switching, saved progress and stale-result rejection. |
| 10 | HTTP/networking: `net`, `net_url`, `net_health`, `net_epoch` | 1,379 + 186 + 167 + 31 = **1,763**, plus included request code | High: dynamic curl loading, HTTPS, redirects, cancellation, pooling and network epochs. Existing C transport can serve earlier Rust modules until this flow is ready. Requires targeted TV validation. |
| 11 | Add-on service: `addons` | **2,479** | High: split manifest/model parsing, installed-list handling and request workers into bounded steps. Depends on earlier URL/JSON/model contracts. |
| 12 | Catalog/metadata/discovery: `catalogo`, `episodios`, `descoberta` | 2,201 + 855 + 7,201 = **10,257** | High: large shared models, asynchronous publication and UI coupling. Extract logic while leaving drawing/interaction in C. |
| 13 | Account/profile/sync: `session`, `profiles`, `account_pending`, `sync` | 727 + 364 + 887 + 2,095 = **4,073** | Very high: auth, logout races, offline intent, reconciliation and account/profile isolation. Complete one operation or state transition at a time. |
| 14 | Live TV/EPG: `xtream`, `xtepg`, `epg`, `stalker` | 654 + 266 + 1,310 + 572 = **2,802** | High: provider-specific protocols, playlist/guide matching, credentials and playback handoff. Split by provider flow; use TV checks for channel playback. |
| 15 | Debrid/plugin services: `debrid`, `plugins`, `pluginjs` | 1,125 + 855 + 1,113 = **3,093** | High: source resolution, credentials, script execution and lifecycle. Keep third-party JavaScript engine/library code and licenses intact. |
| 16 | UI and platform stage: `home`, `detail`, `gfx`, `text`, `tex_cache`, `player`, `video`, `app`, `main`, subtitle modules | Major examples range from **1,471 to 6,522 each** | Very high: start after core parity. Port a screen/component or platform adapter at a time, with visual/input checks and targeted physical-TV testing. Native SDL2/GLES2/media libraries remain dependencies. |

Small files are not automatically easy ports: `net_epoch.c` is only 31 lines,
but publishes changes while holding a mutex and calls stream-fit invalidation.
Conversely, a longer pure text module is a contained, testable change. The
grouping above therefore favors behavioral boundaries over file size alone.

## Next batch boundary

Use English Rust names such as `text_cleanup`, `search_normalization` and
`fold_stylized_character`; retain C entry-point names in temporary adapters.
`limpa` depends on `dobra`, so migrate those together. `buscanorm` belongs in
the same batch because it exercises closely related byte/Unicode boundaries.

Preserve the different existing rules: text cleanup drops invalid bytes while
search normalization substitutes spaces; provider cleanup and local search do
not normalize every character in the same way. Keep the existing allowed
scripts, flag/language markers, separators, case/accent behavior, in-place
editing and UTF-8 boundary truncation. An English UI still accepts non-English
titles, provider descriptions, search input and subtitles.

Carry the `dobra`, `limpa` corpus/font-glyph and `buscanorm` regression cases
alongside the implementation. Add focused Rust tests and real C/Rust adapter
comparisons for malformed input and output capacities where they verify a
contract. Build on Linux, inspect the ARM binary, and use the Linux preview
for search/source display. A new full TV playback session is not a prerequisite
for this pure helper batch; request a TV check if it introduces a dependency,
changes font/rendering/platform behavior or fails to preserve valid content.

## Grouping rule

Several small modules can form one batch when they have a shared purpose or
dependency, have bounded independent contracts and can be tested together.
Grouping is a release/review boundary: implement and verify each part before
integrating the whole flow. Unmigrated dependencies can remain behind the C
bridge; grouping does not require rewriting their entire dependency tree.
Large stateful groups are milestones made of smaller changes, not single
replacement commits. Every batch should leave the hybrid app usable.

## Tests travel with each module

Tests are part of the migration, not work reserved for the end:

1. Preserve the existing behavior cases and fixtures before porting.
2. Add Rust unit tests where they help verify pure rules and error contracts;
   keep C integration/ABI tests while the C UI/services remain callers.
3. Compare C and Rust behavior for the bounded interfaces where feasible.
   Do not treat C bugs as permanent requirements: intentional fixes need
   explicit expected behavior, regression cases and compatibility notes.
4. Translate test-owned identifiers, comments, descriptions and diagnostics
   into English in the same batch. Keep protocol keys, ABI function names,
   serialized compatibility fixtures and multilingual content unchanged.
5. Verify the integrated Linux app and ARM compatibility as appropriate.
   Add concurrency/persistence tests for those behaviors and use visual/TV
   checks for behavior that desktop tests cannot establish.
6. Retire the C reference tests/implementation only after the Rust cases and
   remaining integrations retain the needed coverage.

The completed URL batch already has English C/Rust parity and ABI tests,
plus integration cases running with both implementations. The reused
`tests/addonurl.c` harness is now translated too; its assertions still cover
the same installation/export/reload/request behavior. C service function names
and the expected legacy C diagnostic remain as compatibility interfaces.
The translated runner passes both implementations and the 46,786 direct
C/Rust comparisons; all 20 value and 19 URL assertions are retained.
It is acceptable for an integration harness to remain C during the hybrid
stage: exercising the actual Rust implementation and C callers matters more
than changing the harness language alone.
