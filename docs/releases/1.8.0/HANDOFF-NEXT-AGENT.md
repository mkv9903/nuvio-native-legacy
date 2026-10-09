# Handoff of Nuvio 1.8 Integration — 10/03/2026

## Final Owner Request

Stop upon completing only what was already executing. Do not initiate new tasks. The next agent takes over this document and the plan; do not interpret the existence of the roadmap as authorization for the prior agent to continue. Zero 1.8 releases were published.

## Where to Continue — Do Not Lose Work

- **Integration checkout:** `/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy`
- **Branch:** `codex/integration-180-glass`
- **HEAD:** `e688599116043057f72a83fbee9087cac1ea973d`
- **Many staged, unstaged, and untracked changes exist. HEAD alone DOES NOT contain delivery.** Do not reset, clean, overwrite entire files, or cherry-pick assuming fixes are committed. Review `git status`, `git diff`, `git diff --cached`, and untracked files.
- Original user checkout: `/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy`, branch `fix/tpk-selo-hdr`, also dirty. Preserved. Do not confuse with integration.
- Untracked `saida=` has unknown ownership; do not delete.
- Internal disk is constrained; builds and snapshots reside in `/Volumes/ExternalSSD`.
- Complete plan: [PLAN.md](PLAN.md). Inventory: [INVENTORY.md](INVENTORY.md), [INTEGRATION.md](INTEGRATION.md).
- Execution history: `.superpowers/sdd/PLANO/progress.md`. Chronological: latest entries supersede older ones.
- Owner references: `docs/HANDOFF-1.8.0.md` and `/private/tmp/nv-codex-ctx/docs/glass-ui-contexto.md`.

## What Runs on Android TV

Smart TV Pro, Android 14, `192.168.1.128:5555`. Authorized install via `adb install -r`, preserving data. APK armeabi-v7a and arm64-v8a; TV uses armeabi-v7a. Preview version **1.8.0 / 10800**, not public release.

- Immutable source: `/Volumes/ExternalSSD/nuvio-180-preview-ui-home-final`, `manifest.json` with 1,275 hashes of src/tests.
- Packaging copy: `/Volumes/ExternalSSD/nuvio-180-android-preview`.
- Local packaging commit: `6fa8e2d4d8abb9c8b28054d720e9729f8bd7ca2d`.
- APK: `build/release-1.8.0/Nuvio-1.8.0-android.apk` in that copy.
- SHA-256 produced and extracted from TV, matched by coordinator: `8d012314de5b73d60958dec7f06c1f6620abd7842550d8308f66fb46b03216d4`.
- Inspected physical screenshot: `build/release-1.8.0/android-tv-home-settled.png`; Home with CW, friends, and For You.
- Details: [ANDROID-PREVIEW.md](ANDROID-PREVIEW.md).

Includes Details 1.7.4 restoration, continuous Apple TV veil, 1.7.4 ratings, separated cast/recommendations, progressive initial Home, #158/#233/#228 reproduced fixes, diagnostics/persistence, local preference protection, Seekr Settings/quota, and initial N01. **Does not include Seekr OSD or F03 wiring completed after freezing source.**

Spaced screenshots do not measure startup latency. Zero comparable 1.7.4/1.8 benchmark on TV or full playback/fluidity proof. Luna used monkey to launch; jumping from selector to Home does not prove auto-resume. In future runs, resolve component and use `am start` without random events. Zero new physical Samsung/LG tests.

Build copy has adjusted version; integration retains legacy versions in some files. Do not synchronize version/release from preview number. Prior build picked up stale objects because copy2 preserved timestamps: invalidate objects or touch all sources before recompiling copy.

## Completed in this Integration, with Explicit Limits

| Track | Delivery | Proof / Limit |
|---|---|---|
| Details / Apple TV | Visual restoration v1.7.4 preserves fan, episodes, navigation, integrations; logo first, buttons below; foreign title subtitle without waiting for decode; no inline cast | Local GL and review; do not claim all UI physically approved |
| Veil | Expansion endpoint uses continuous composition, including Immersive/Frost/blur/poster/no artwork | 7 GL scenarios; p95 diff ≤ 1/255; [CONTINUITY-VEIL.md](CONTINUITY-VEIL.md); native video/trailer and real GPU pending |
| Ratings and Rows | Render notasui v1.7.4, media API preserved; cast followed by recommendations in movie/show | 10 items, focus/scroll past 7th, pointer, TMDB tv/movie route; [DETAIL-ROWS-REVIEW.md](DETAIL-ROWS-REVIEW.md) |
| Home Loading | CW/social only as initial state: first row can appear without waiting for last; ready cache/lists/collections do not collapse | Controlled before ready 1ms / visible 354ms; published immediately upon ready; 5 scenarios + legacy ASan/UBSan. Not measured hardware gain. [HOME-LOADING-IMPLEMENTATION.md](HOME-LOADING-IMPLEMENTATION.md), [review](HOME-LOADING-REVIEW.md) |
| #233 | Stable ID collections, valid empty snapshot removes; invalid JSON/RPC does not delete; consistent editor and Home; collection outside catalog quota | Parser/projection/Home/concurrency and review. Current reporter/Samsung TV logs pending |
| #158 | Subtitle add-ons do not collapse by provider; local CW preserves offline metadata/log | Reproduced/tested fixes; unproven whether they explain specific Flix-Streams or all user logs |
| #228 | Trailer fallback and prepared/playing states fixed in reproduced paths | Focused suites; physical Samsung Q80A pending |
| Memory Diagnostics | Separates candidate, applied budget, persisted, rollback, manual override; handles disk failure / timeout / commit | 19 cases, ASan/TSan, real cache test; did not raise hardware caps or prove gains |
| Per-Device Profiles | Incoming blob respects `somenteDesteAparelho`; local RAM/quality/GPU do not follow other TVs | `ajustes_perfil` PASS; existing `cliente.txt` UUID reused; two-TV test pending. [DEVICE-PREFERENCES.md](DEVICE-PREFERENCES.md) |
| Seekr Engine/Settings | Personal key, 50 queries per install / UTC day shared across profiles/keys; persistence, cache/singleflight, separate 429, local retry, write failures | Quota/states/Settings suites; WGT Retry-After exposed or unknown |
| Seekr OSD, UNINSTALLED | Capsule during scrubbing with cause, usage/reset; without separate key/clock/storage/provider; dismisses on release | Unit test rerun from root + 16 glass/solid GL captures; root inspected quota. [F02-SEEKR-PLAYER-OSD-DELIVERY.md](F02-SEEKR-PLAYER-OSD-DELIVERY.md) |
| Initial N01 | Isolated request API, TLS, caps before allocating, leak-free redirects, deadline, cancel/generation, metrics | Local HTTP/TLS, ASan/UBSan/TSan, TPK 40 host and wrappers; root reran test. [NETWORK-REQUEST-CONTRACT.md](NETWORK-REQUEST-CONTRACT.md). F03 below extends this API |
| Library/Guide | Selective library fade/crop port, Guide Unicode confirmed | GL fixture and `jscadeia` test |

The OSD added narrow getter `ajustes_seekr_habilitado()` (local option), keeping `ajustes_seekr_ligado()` (option+key) for dispatch. Files: `player.c`, `ajustes.c/h`, and four `tests/player_seekr_status*`. Zero new provider requests.

## Last Completed Task Before Pause: F03

**Stable, uninstalled:** real Android epoch via ConnectivityManager/JNI and media diagnostics wired to engine per final host. N01 received bounded disposable mode and uses CA bundle configured at startup. Player/mini block or cancel active test. Android uses conservative single connection; LG/TPK without reliable epoch and WGT without N01 contract remain unknown for StreamFit, keeping legacy diagnostics.

ASan/UBSan, TSan, Kotlin/SDK 35 callbacks, NDK arm64/ARMv7 syntax, and focused regressions PASS. Coordinator verified reports/logs and cancel/epoch hunks; **full integrated review, full-core/APK, and hardware proof of this delta remain pending**. Runtime metadata/playback and passive telemetry NOT started. Without valid duration, classification may remain unknown: do not announce end-to-end selection ready.

Delivery and exact file/command list: [F03-EPOCH-DIAGNOSTICS-HANDOFF.md](F03-EPOCH-DIAGNOSTICS-HANDOFF.md). Production: `NuvioActivity.kt`, `app.c` (cancellation), `diagnostico.c/h`, `rede.c/h/rede_pedido.inc`, new `redemarca.c/h` and `streamfitdiag.c/h`. Does not modify `NvPlayer`/`ParaleloDataSource`/`streams` runtime in this slice.

## Remaining Items, in Resumption Order

1. **Review and Consolidate Dirty State:** compare staged/unstaged/untracked, integrate new reports/fixtures; selective commits per track and final merge remain needed. Do not repeat full-file ports. Perform integrated build from frozen source containing latest deltas, as TV runs prior source.
2. **Complete F03 StreamFit:** genuine runtime per exact target (movie/episode), backend duration without `PLR_DUR_PADRAO`/trailer; real duration overrides delayed metadata. Passive Android `TransferListener` telemetry pending; do not use unsynchronized sum of parallel downloads or cache/pause/full buffer as throughput. Expose origin/age/budget/unavailability in translated format. Do not generalize resolver host or network via constant key. Preserve order/groups/focus and first-source mode.
3. **F04 Subtitles:** audit/contract only, zero new F04 code. [F04-SUBTITLES-AUDIT-CONTRACT.md](F04-SUBTITLES-AUDIT-CONTRACT.md). Current selection in `faixas.c`, `legendasui.c` does not exist. Simple UI language+origin/embedded/addon and More options; stable candidates under late arrival. Second subtitle needs independent loader/document/render and collision handling; current ASS/mkvass are singletons. Do not offer non-rendering secondary option.
4. **F05 AutoSync:** engine/documents/562 tests exist; independent embedded reference collector and player wiring, sessions/cancel, Quick/Thorough/tolerance/undo/another reference pending. Without blocking startup; apply proven confidence only; first single language, then dual. Document: [AUTOSYNC-IMPLEMENTATION.md](../../plans/player-1.8/AUTOSYNC-IMPLEMENTATION.md).
5. **F06 Audio:** prove PCM per backend before VAD/optional model; explicit download, hash/size/cancel/remove, RAM/latency/confidence. Do not silently disable passthrough/offload. LG/Samsung unproven for accessible dialogue.
6. **F07 Cache/Boost:** implement/prove temporary cache 256/512/1024MB, Range/auth, cancel, ENOSPC, crash/cleanup; volume up to 200% per session, reset, and red >100%. AVPlay internal buffer is not file cache. Real capability per platform is F00 gate.
7. **F08 Social:** consolidate Nuvio account/profile independent of Trakt/Simkl; deduplicate channels/friends and associate verified identities. Complete movie/show/genre/likes/match/progress comparisons with true coverage and unknown states. Review/deploy backend/migration pending; do not assume Simkl/Letterboxd APIs fully integrated. Horizontal card / exterior menu / online green require final QA.
8. **F09/F10 Plugins/P2P:** selective port of plugins2/p2p2 after reviewing N01; global JS/DOM/queue limits, cancel/generation/auth; P2P shutdown before clearing writers, RAM/disk/statvfs/watchdog. Toolchains/motor/licenses LG/Android/TPK; WGT lacks local sockets. Do not label inventory complete implementation.
9. **F11 Personal Media Servers:** research complete in `docs/plans/media-servers-1.8`; Jellyfin implementation pending (auth/library/PlaybackInfo/profile/DirectPlay/progress), followed by dedicated Emby/Plex. Origin/user/ID isolation and authenticated sessions.
10. **F12 TopN/Icons/Scripts:** check remaining true deltas, pagination/cost, optional Android icons, and issues 226/234; Mac/Linux scripts and paths with spaces. Items remain in inventory.
11. **QA and Platform Performance:** measure Home→rows, Play→first frame+audio, and remote→response; frames/decodes/uploads/RAM/cache/scroll/fan/Frost, before/after with equivalent conditions. Benchmark Android and LG/Samsung separately. Check Apple menu, Modern sidebar, Guide/addons/categories/source/friend modal, Settings scale, and details/heroes without regression. Static captures do not measure fluidity.
12. **Logs/Issues/Release:** restore access to current worker logs, correlate build/timestamp/device before replying/closing #158/#228/#233 and remaining issues. Review open items #229/#231/#232/#227 and PR 230 against current code, do not assume remote closure. Update What's New and final versions solely when real scope ready; signed builds per target, hashes and updater/Homebrew, smoke tests, tag/release/artifacts from identical commit, and honest issue replies. Zero releases during this pause.

## Tests/Evidence for Resumption

Commands in integration, or matching snapshot when concurrent WIP exists:

```sh
bash tests/detail_layout.sh
NV_ROWS_ONLY=1 NUVIO_SHOT_IDIOMA=0 bash tests/detail_secoes_shot.sh /tmp/nuvio-rows
bash tests/player_seekr_status.sh
bash tests/ajustes_perfil.sh
bash tests/diagnostico.sh
SANITIZE=1 bash tests/home_progressiva.sh
SANITIZE=1 bash tests/montagem_cedo.sh
bash tests/rede_pedido.sh
```

Before repeating expensive tests, check logs and modified sources. GL requires graphical context on Mac. TPK 40 host/syntax does not prove ARM or TV. F03 reports must append their specific commands.

Useful snapshots: `/Volumes/ExternalSSD/nuvio-180-ui-continuity` (final UI), `/Volumes/ExternalSSD/nuvio-180-execution-51fxfkxd` (previous cohort), `/Volumes/ExternalSSD/nuvio-174-detail-reference` (v1.7.4 baseline). `/tmp` contains captures/logs, may be volatile; versioned references reside in `docs/releases/1.8.0`.

## Constraints and Coordination

- M3 localhost:18080 refused connection; memory_write attempted, not saved. Zero confirmed remote memory from this delivery.
- Bitwarden `bw status`: unauthenticated. D1 previous Error 7403; do not assert fresh worker log reads. Secrets never in handoff/logs.
- User authorizes subagents and models by cost: Luna for build/collection/docs; Sol for code/integration/review; Astra targeted for high-risk contracts, not simple tasks.
- Android Luna chat: `01a10412-0de7-7480-9bd5-55a436a37c6c`, finished; will not perform further installations.
- Preserve visual feedback: legacy Details layout; logo first, buttons below; fan expands on focusing first functional button; carousel does not swap title while expanded; cast separated from recommendations; do not recreate rejected redesign.

## Recovery Checkpoint

`/Volumes/ExternalSSD/nuvio-180-handoff-stop-u4ldpf03` contains HEAD/status, separated staged/unstaged patches, copies of modified files and new sources/tests/docs, with SHA-256 manifest. Not full clone; using integration checkout is primary path. Do not reapply patches over it. Implementation agents terminated; Android chat idle. Old slot `issue158_fix` remained pending_init and received interrupt, zero new execution.
