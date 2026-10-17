# Search and expanded catalog browsing

Feature review requested on 2026-10-17. Implementation has not started; wait for
the user's keep/remove answers. The application checkpoint remains `78137d87`,
with its package and validation recorded in [Home coordination](home-coordination.md).

## Existing features for review

Search combines built-in Nuvio movie/series catalogs, optional Cinemeta fallback
and installed addon catalogs that declare search support. It also matches titles
already loaded locally. Results arrive progressively in catalog/provider rows,
with a Best Result card. Short/empty queries show Popular suggestions. People
results combine matching loaded cast with the shared TMDb person-search service;
selecting a person opens the existing filmography flow.

The device remembers ten confirmed searches per profile, newest first. Repeated
queries move to the front; individual removal and clear-all are supported, and
logout removes history. The current C history uses byte-sized buffers and old
text files; none of those files will be imported by the Rust implementation.

Expanded browsing supports addon catalogs, genre filters and near-end page
loading. It deduplicates overlapping pages, stops on confirmed exhaustion, retains
loaded results after a failed page, supports retry and restores the loaded list
when returning to the same source. Collections add source tabs, direct TMDb and
Trakt list/discovery sources, ranking order and chronological filmographies.
Opening titles, poster actions, the existing information panel and navigation
back to the same position are part of the retained C screen behavior.

Keep the existing on-screen/physical keyboard and platform text-entry actions.
Use English UI/code/comments/names as already agreed. Valid Unicode titles,
provider IDs and query text remain data; providers receive the original query,
while local matching uses normalized text. UI appearance and navigation are not
redesigned in this milestone.

## Proposed Rust boundary, subject to the feature answers

Move search target discovery and eligibility, query state, provider requests,
parsing, grouped immutable results, local matching and recent-query storage into
Rust. Replace the C fixed target/query/request buffers with Rust models and small
native actions/views. Bound background workers and completed replies for the TV.
Profile, addon, query and request generations must reject obsolete results.

Move expanded-list state, source identity, pagination, duplicate handling,
retry/end status, genre encoding and ordering into Rust. If retained, include the
TMDb/Trakt browsing requests and person-search requests used by these flows;
this does not complete their broader tracking/enrichment integrations. Retained
Details and collection services can consume complete Rust records until their
own rewrites. Translate each complete touched C UI component while preserving
its rendering, illustrations, layout and remote actions.

Only this rewrite's versioned private Rust JSON is eligible for restoration.
Scope history/results to the member, owner and selected profile; clear only this
capability's private files on logout. Do not add original-file readers or
compatibility import paths. Reuse the existing Rust HTTP, addon/manifest, title
ownership and guarded publication foundations rather than copying C parsers.

## Existing cases and planned Rust coverage

- Translate `tests/buscanorm.c`: case/accent matching, valid non-Latin text,
  invalid UTF-8, character counting, complete-character truncation/deletion.
  Preserve the distinction between local normalized matching and original
  provider query text.
- Translate `tests/buscasrec.c`: minimum length, trimming, ordering, duplicate
  promotion, ten-entry retention, restart, remove/clear, isolation and logout.
  Test Rust JSON instead of copying the old text-file fixture protocol.
- Inventory `tests/metaprov.c` search/fallback cases before replacing that path.
  Cover the Cinemeta preference and addon changes invalidating a repeated query.
- Add provider and native integration cases for late/failed replies, cancelled
  queries, profile switches, long configured URLs/IDs/queries, incomplete and
  empty payloads, incremental publication, title ownership and bounded workers.
- Add pagination cases for variable page sizes, raw versus accepted item counts,
  overlapping/repeated/empty pages, retry after failure, source/filter switching,
  generation cancellation, retained position, collection source tabs and ordering.
- Retain the visual/navigation assertions in `tests/busca_shot.c` and
  `tests/vertudo_shot.c` as C UI checks, adapting their data access to Rust.
  Live TV guide-search pagination fixtures are a separate backend area.

Run the relevant Rust, native, memory and Linux checks for the completed area,
push tested checkpoints, then build the isolated package before a manual handoff.
