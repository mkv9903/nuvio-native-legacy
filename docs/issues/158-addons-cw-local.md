# Issue #158 — Subtitle Aggregation and Local Resume in 1.8 Integration

Date: 10/03/2026. Worktree `codex/integration-180-glass`. Local fixes; zero installation, release publication, tag/attachment modifications, or issue comments/closures occurred during this step.

## Reproduced Defects and Current Behavior

| Path | Before | After | Proof |
|---|---|---|---|
| Subtitles from multiple addons | `buscarLegendas` terminated outer loop once first addon filled `LEG_MAX=12`; language quotas were consumed prior to querying subsequent addons | Queries all active addons declaring subtitle resources. Buffers bounded batch per source and distributes 12 slots via round-robin within each language group | Two providers, each with 56 candidates. Both queries execute and each contributes six rows; primary/secondary/English receive four rows each |
| Source in track selection sheet | All external subtitles displayed OpenSubtitles badge, including results from other addons | `Legenda.provedor` follows result into `rotuloLegenda`; missing source defaults to "External subtitle" | Track sheet label function verified with OpenSubtitles, Subs.ro, and Community Subtitles |
| Local Continue Watching | Constructed blank `CatItem` from progress and relied on Cinemeta/Trakt enrichment. IDs without `tt` and missing posters were compacted out | Copies metadata from catalog/cache via base identity + type under `pubTrava`. Active progress restores ID, episode, percentage, timestamp, and remaining runtime. Enriches clone; remote cache evictions do not purge local records | IMDb, TMDB, Kitsu, and custom provider preserve title/poster/source while offline. Un-cached entries retain "Movie"/"TV Show" placeholders without synthesized art |
| Network restoration | Persisted placeholder title could prevent parsers that only populate empty fields from loading real title | Placeholder stripped only on copy sent to enrichment; real result overwrites placeholder on arrival | Offline fixture → catalog with placeholder → reconnection delivers real title |

Distribution preserves primary → secondary → English ordering. "All" and missing preferences use an unfiltered group. Visual cap remains 12; does not guarantee slots for all when available sources exceed capacity. Temporary batch is heap-allocated and bounded by `ADD_MAX * LEG_MAX`, avoiding large stack allocations on TV hardware.

`cat_copiar_por_id` compares full base identity (`kitsu:41370`, `tmdb:t101`, etc.) without truncating at first `:`; filters by type and prefers cached entries with posters. Returns independent copy under publication lock, ensuring discovery does not hold dangling pointers to released memory blocks on frame boundaries. When episode shifts, stale episode title is cleared.

Local Continue Watching criteria remain: minimum duration 60 s, at least 1% completed, and percentage below `ajustes_cw_concluido()` (factory default 90%). Zero changes to write gate `video_pronto >= 120`. Hiding or removing a collection does not delete progress records; local test preserves all five records even with an empty catalog. Explicit CW sorting and removal also pass existing regression suites.

## Per-Resource Diagnostics

Subtitle query uses `rede_baixar_medido_controle` and outputs:

```text
[addon-recurso] addon=2 resource=subtitles tipo=movie http=200 bytes=4007 ms=7 array=1 recebidas=56 candidatas=12
```

`addon` is list index, `tipo` is movie/series/other, and remaining fields are metrics/counts. `array=1 recebidas=0` distinguishes empty arrays from non-arrays. `candidatas` tracks accepted batch prior to slot allocation. Completion summary omits title ID. URLs, media IDs, filenames, payloads, configuration, and headers are excluded from these telemetry events; test fixture asserts absence of hostnames, mock tokens, and synthetic IDs on stdout.

This event alone does not determine root cause of HTTP 0 statuses, validate full JSON payloads, or prove live server responsiveness.

## Local Validation

Executed successfully:

```sh
bash tests/addons_legendas.sh
SANITIZE=1 bash tests/addons_legendas.sh
bash tests/cwlocal.sh
SANITIZE=1 bash tests/cwlocal.sh
bash tests/cwordem.sh
bash tests/cwremover.sh
```

`addons_legendas.sh` exercises the real worker and track sheet labels: Romanian ISO language, sorted preferences, dual dense sources, correct origin attribution, empty responses, missing resource declarations, HTTP 404 handling, mandatory fields, and episode matching. `cwlocal.sh` exercises `continuarLocal` with mock enrichment that drops un-cached art or simulates total network outages; verifies base + type, saved metadata, un-cached progress, episode changes, reconnection, and original bounds. Metadata copies remain valid after catalog recreation and block retirement.

Before/after validation exported `HEAD:src/addons.c` and `HEAD:src/descoberta.c` to temp folders without overwriting worktree files. Prior worker failed the dual query/contribution assertion; prior local assembly failed the 5-record retention assertion. Fixed codebase passes all scenarios with ASan/UBSan.

Existing CW suites pass completely (rules, discovery, and Home). Added minimal doubles for new integration symbols: recommendations generation, modal states, and account badge packs. These doubles do not replace tested CW logic. Existing compiler warnings: `VideoLegendaEstilo` initializer without `negrito`, and `fim` variable in `colecoes.c` used only in assignment.

## Flix and Issue Closure Prerequisites

Historical evidence documented in [ISSUE-158-ADDON-LOGS.md](../releases/1.7.3/ISSUE-158-ADDON-LOGS.md). HTTP 404s in legacy logs do not distinguish resource or show root cause. In this integration, new D1 query failed with Error 7403 / unauthorized account; no new log attributable to reporter. Private URLs were not executed.

Reproductions resolve aggregation bugs, origin attribution badges, and local metadata evictions; they do not prove that Flix, Pasha, Subs.ro, or Community addons installed by reporter respond correctly today. Live verification on release candidate is required, capturing exact manifest/resource/status and disambiguating Flix-Streams Free from identically named addons. Without this, Flix root cause remains unconfirmed.

For 1.8: validate subtitle sheet and resume on verified hardware builds; observe behavior with user's installed addons; capture sanitized logs of Flix requests before claiming complete resolution. Desktop fixtures, package verification, and physical hardware testing represent separate validation gates.
