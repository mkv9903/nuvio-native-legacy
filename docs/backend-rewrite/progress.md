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
| Manifest models, validation, provider requests and document cache | Rust services integrated; native manifest scheduling remains C |
| Account Home ordering and preferences | Rust backend integrated; local row registry remains C |
| Home catalog title/row models, ownership and private scene JSON | Rust backend integrated |
| Home request scheduling and row assembly | Transitional C orchestration using Rust requests, records and scope guards |
| Search and expanded catalog browsing | C backend remains |
| Details metadata, episodes and enrichment/artwork services | C services remain; complete title models and rating helper are Rust |
| Playback source aggregation and selection rules | C backend using Rust-owned addon data |
| Subtitle fetching, parsing and synchronization | C backend using Rust-owned addon data |
| Progress, resume and Continue Watching | C services with Rust context/ownership guards |
| Saved items, library, lists and collections | C backend remains |
| Trakt and Simkl integration | C backend remains |
| Live TV providers, channels and EPG | C backend remains |
| General settings, credentials and remaining utilities/integrations | C backend remains; rewritten capabilities use their private Rust storage |

The latest application correction is `204511dd`. It removes the local addon
sharing override and pins the Home worker's addon snapshot after the account
wait. The user reported that the corrected behavior seems fine on 2026-10-17.
That report does not establish a new TV-runtime validation result.

Recorded automated checks for that source are 196 backend tests, 45 native
integration tests, 37 native ASan/UBSan cases, retained settings regressions,
Linux build/startup and ARM/IPK verification. See
[the profile regression record](profile-catalog-regression.md) for exact package
identity, source and hashes. This progress update changes documentation only.

## Recommended next area

Finish Home loading coordination before starting another user-facing backend
area. Rust already owns the catalog requests and records; C still schedules
manifest/catalog work, chooses candidates, assembles rows and merges the other
services' contributions. Moving this coordination reduces the remaining
cross-language state involved in the profile flow just tested.

Retain the C screen design and existing service contributions while replacing
this orchestration with Rust collections, bounded workers, immutable snapshots
and versioned private storage where this area owns saved state. Continue
Watching, collection and tracking service implementations can supply scoped
inputs until their own rewrites. Do not import their original saved files.

The feature review for this area is pending:

- Progressive rows during the first load and a stable cached Home during
  background refresh.
- Row limits, saved order, disabled rows and selection of further eligible
  catalogs when earlier candidates return empty.
- Collection-row composition and integration of Continue Watching and saved-list
  data, using the existing services until those are rewritten.
- Immediate removal of rows from removed or disabled addons.
- A separate choice about the existing automatic placement of new-addon rows.
  The recommendation is to preserve saved order and append new rows; removing
  automatic promotion requires the user's feature decision.

No implementation of this next area starts before those feature answers arrive.

## Validation plan for Home coordination

Inventory the retained cases before replacement, including manifest cache/wait,
discovery start races, progressive Home, row quotas/ordering, duplicate rows,
collection composition and profile isolation. Translate rewritten assertions
into Rust; retain C cases only for capabilities still implemented in C.

Cover empty and delayed responses, failed-load cache retention, selected-profile
first startup, independent/shared profile switches, cancellation during assembly,
bounded concurrency, stable refresh publication and final owner release. Verify
the selected new-addon placement policy rather than preserving that algorithm
implicitly. Run backend/native/Linux checks and relevant sanitizers, push tested
checkpoints, and package the next manual-test milestone.

## Subsequent areas

The proposed sequence after Home coordination is search/expanded catalogs,
Details metadata and episodes, source aggregation, progress/Continue Watching
and saved data, subtitle services, tracking integrations, Live TV/EPG and remaining
settings/utilities. Dependency findings may change this order. Review each
area's features with the user before implementation and pause for feature
decisions, even when the implementation recommendation is straightforward.
