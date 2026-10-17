# Backend rewrite progress

Updated 2026-10-17 on `codex/rust-backend`.

The planning estimate is **about 30% complete**, with a rough range of **25–35%**.
This estimates integrated backend capabilities and the remaining implementation
work. The retained C UI, SDL/GLES rendering and LG Luna/native playback bridge
are excluded. Shared Rust models and native ownership are useful foundations,
but they do not make their remaining C service implementations complete.

## Current implementation

| Area | State |
| --- | --- |
| Account login, sessions, refresh, profiles and PIN rules | Rust backend integrated |
| Addon account lists, edits, offline journals and synchronization | Rust backend integrated |
| Manifest models, validation, provider requests and document cache | Rust services and Home manifest scheduling integrated |
| Account Home ordering and preferences | Rust backend integrated; local row registry remains C |
| Home catalog title/row models, ownership and private scene JSON | Rust backend integrated |
| Home request scheduling and row assembly | Rust planning, bounded loading and catalog assembly integrated; C service contributions retained |
| Search and expanded catalog browsing | Rust text and per-profile recent-query storage integrated; provider search and expanded browsing remain C |
| Details metadata, episodes and enrichment/artwork services | C services remain; complete title models and rating helper are Rust |
| Playback source aggregation and selection rules | C backend using Rust-owned addon data |
| Subtitle fetching, parsing and synchronization | C backend using Rust-owned addon data |
| Progress, resume and Continue Watching | C services with Rust context/ownership guards |
| Saved items, library, lists and collections | C backend remains |
| Trakt and Simkl integration | C backend remains |
| Live TV providers, channels and EPG | C backend remains |
| General settings, credentials and remaining utilities/integrations | C backend remains; rewritten capabilities use their private Rust storage |

Before this milestone, application correction `204511dd` removed the local addon
sharing override and pinned the Home worker's addon snapshot after the account
wait. The user reported that the corrected behavior seems fine on 2026-10-17.
That report does not establish a new TV-runtime validation result.

Recorded automated checks for that source are 196 backend tests, 45 native
integration tests, 37 native ASan/UBSan cases, retained settings regressions,
Linux build/startup and ARM/IPK verification. See
[the profile regression record](profile-catalog-regression.md) for exact package
identity, source and hashes. The current Home milestone is described below.

## Home coordination checkpoint

The user approved all reviewed Home features and removal of automatic new-addon
promotion. Rust now plans and loads Home catalogs, preserves saved declaration order,
fills empty slots and assembles immutable catalog scenes. Failed preferred rows
retain their retry positions across refreshes and restarts. C screens retain their
appearance and the contributions of still-C collection, progress and tracking
services. See [Home coordination](home-coordination.md) for boundaries and cases.

This is one completed backend coordination area; the estimate remains about 30%
until the larger remaining service groups are implemented. General settings and
the local row editor have not become Rust services in this step.

Application source `78137d87` is pushed and packaged for manual tests.
It passes 214 backend tests, 53 native integration tests, 45 native ASan/UBSan
cases, Linux build/startup and ARM/IPK verification. The exact source, package
hash, build ID and focused manual cases are in the linked Home coordination
record. TV runtime testing is pending.

## Subsequent areas

The proposed sequence after this Home test checkpoint is search/expanded catalogs,
Details metadata and episodes, source aggregation, progress/Continue Watching
and saved data, subtitle services, tracking integrations, Live TV/EPG and remaining
settings/utilities. Dependency findings may change this order. Review each
area's features with the user before implementation and pause for feature
decisions, even when the implementation recommendation is straightforward.


The next [search and expanded-catalog inventory](search-and-catalogs.md) is ready.
The user removed People search from both Search and Spotlight, kept Details cast
and filmography navigation, and kept all reviewed expanded-browsing features.
The next milestone is in progress. Rust text handling and per-profile recent-query
storage now feed both C search interfaces, and People search is removed. Provider
search, query ownership, grouped results and expanded browsing remain pending.
This smaller integrated area does not change the approximate 30% estimate.
