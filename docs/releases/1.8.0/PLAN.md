# 1.8.0 Closure Plan

10/03/2026. Executable plan for integration, fixes, performance, validation, and delivery. Current public release is [1.7.4](https://github.com/iqui27/nuvio-native-legacy/releases/tag/v1.7.4). This document does not declare 1.8 ready. It replaces the version groupings from the 10/02 roadmap; that file remains as historical reference.

## Actual Base State

- Working branch: `codex/integration-180-glass`, outside owner's main checkout.
- Merge Glass + published master: `b98bd2f5`. Monthly Agenda integrated by other conversation: `e6885991`. Additional changes from multiple agents remain in index/working tree.
- Glass, 1.7.4 fixes, dedicated Settings scale, menus, island, player, Where to Watch, and part of Social are already in the base. This does not eliminate integrated and physical validation.
- PR [#230](https://github.com/iqui27/nuvio-native-legacy/pull/230) was already merged at `66af5817` and is in this base. Track selection re-test on Samsung remains pending; do not re-merge the same PR.
- AutoSync has time engine and tested documents, but player wiring, independent embedded reference, and second renderer remain missing. It is not available to the user just because this code exists.
- Seekr has personal key and limit of 50 `/sprites` calls per install/day, shared across accounts/profiles/keys. State/usage messages, diagnostics, and Retry-After handling in WGT are missing. Reinstalling/clearing data wipes local protection; enforcement after that requires backend.
- #228: reproduced fallback fixes and native states; #158: reproduced and fixed subtitles and local CW. #233 is under implementation. Stream-fit has engine/sheet with focused tests, but diagnostics/runtime/network integration and passive Android telemetry remain pending.
- New worker logs: access blocked by Cloudflare D1 Error 7403. Vaultwarden CLI unauthenticated. Neither error identifies root cause of user reports.
- No final 1.8 package, publishing, or deploy of Social backend has been made for this integration.

The [merge inventory](INVENTORY.md) breaks down ancestral branches, divergent patches, and uncommitted changes. Number of distinct commits does not equal number of new features.

## Parallelism and Model Selection

The goal is to use concise context and verifiable results. Up to three concurrent executors in this chat, alongside coordinator; Android chat is an independent task authorized by owner. Each task receives responsible files, existing contracts, test command, and completion criteria. Agents do not revert other agents' work.

| Work | Model / Effort | Concrete Usage |
|---|---|---|
| APKs, inventory, notes, translations, and simple fixture adaptation | **gpt-6-luna**, low/medium | Strict checklist; preserves signature, data, and versions; scales to Sol if runtime defect occurs |
| Common C/Android implementation, UI on top of existing design, tests and reproduced fixes | **gpt-6.1-sol**, medium; high when concurrency involved | Modular execution and focused validation |
| Social identity contracts, plugin sandboxing, P2P cancellation, AutoSync/audio confidence | **gpt-6-astra**, high | Targeted review of risks and difficult decisions; implementation returns to Sol |
| Integration closure | Coordinator + Sol for independent review | Dependencies, conflicts, evidence, and packaging from same commit |

Do not pass entire conversation history to every executor or ask multiple agents for the same investigation. Reuse results. Luna does not solve race/threading issues alone without review. Astra will not be used for translation, note generation, or mechanical inventory. Do not estimate costs in dollars without consumption/pricing data; constrain costs via size and count of tasks.

## Sequence and Deliverables

Waves organize execution; pending features are not automatically removed from requested scope. A capability that fails on a given backend must be implemented/proven or have its limitations documented explicitly before any announcement.

### Wave 0 — Preserve, Inventory, and Consolidate

| ID | Task | Model | Deliverable / Completion Condition |
|---|---|---|---|
| G01 | Check branches, patch equivalence, and loose changes, including main checkout and older worktrees | Luna + Sol review | Manifest `origin -> files -> integrated/duplicated/superseded/pending -> tests`; no work lost |
| G02 | Consolidate current agents into commits by responsibility | Sol/coordinator | #158, #228, #233, Seekr, AutoSync, and layout into reviewable commits; resolve overlaps in `home.c`, `descoberta.c`, `streams.c`, network, and translations |
| G03 | Import only useful deltas for icons, Guide Unicode, Library, Social documentation, and other worktrees | Sol | Confirm behavior is still missing in base; avoid replacing newer modules with older versions; test each delta |
| G04 | Record a frozen source for each test build | Luna | Commit/tree, included/excluded feature list, package/hash, signature, target, and installed build |

Intermediate Android build was installed via Luna chat on Smart TV Pro/Android 14, preserving data; version 1.8.0/code 10800 confirmed on device. Snapshot `38b0efb0` includes stable hunks from index on top of `e6885991`; excludes #233, #158, and stream-fit WIP present at capture time. Test version does not equal published release. This build serves owner testing, not the final candidate for this sequence. Minimal comment fix in `socialvis.h` discovered during Android build `701d4113` was ported to integration; local version change `098cd245` remains strictly in preview copy.

### Wave 1 — Bugs, Logs, and Base Stability

| ID | Task | Model | Deliverable / Completion Condition |
|---|---|---|---|
| B01 | **#233 — account, collections, order, and row editor** | Sol/high | Valid empty snapshot removes collections; empty error/RPC does not erase state; Home and editor show same IDs/order/hidden; delete does not promote arbitrary rows; collections do not consume catalog quota; safe concurrent publication |
| B02 | **#228 — Home/hero/poster trailers** | Sol + lifecycle review | WGT/TPK tests and `Prepared -> Playing`; fallback uses real order and does not re-open failed source; correct timeout/pause/ROI; Samsung retest with identified source/build |
| B03 | **#158 — subtitles, Flix, and Continue Watching** | Sol | All subtitle sources queried, correct provider; CW works offline with IMDb/TMDB/Kitsu/custom IDs and preserved thresholds. Stream diagnostics distinguishes unmade/HTTP/empty/parse/rejection request. Flix requires real manifest/resource/log before attributing cause |
| B04 | **#223/#211/#188/#197 — startup, playback, and catalog** | Sol/high; Astra only for difficult causality | Reproduce scenario per device, lifecycle/error, and version. Fix confirmed defect; retain unconfirmed case as known issue. No universal claims based on another TV |
| B05 | Recover log access and analyze current slice | Luna for sanitized collection; Sol for diagnostics | Read-only SELECT by timestamp/platform/build; separate session upload, old log, and diagnostic attempt. Link events to issue before responding |
| B06 | Enhance new logs and performance reporting | Sol | English events with stage, generation, result, HTTP/error, bytes, and duration; startup/seek/rebuffer, memory/cache/disk, Seekr, and AutoSync. No keys, headers, signed URLs, or private payloads |

B05 may proceed in parallel with reproducible tests. Cloudflare D1 blockage does not halt all implementation, but prevents claiming new logs were read or that a fix resolves that specific report.

### Wave 2 — Authorized 1.8 Features

| ID | Task / Dependency | Model | Remaining Work |
|---|---|---|---|
| F00 | **Small capability proofs per backend and baseline** — before F06/F07/F10/F11 | Sol for spikes; Astra only when needed | Capability/device/owner/experiment/evidence matrix: PCM, embedded extraction, file/Range/auth cache, gain, engine/toolchain, and authenticated media. Record enabled/unavailable/blocked and work needed to meet request; do not wait until Wave 4 to discover structural blockers |
| N01 | **Network infrastructure and per-call jobs** | Sol/high; short Astra review | Ownership `rede.c/h` and workers/adapters; deadlines/cancellation/generation, byte caps before allocation, authenticated redirects/headers, native/WGT transport. Port generic API without TLS/TPK40 regression. Tests for cancel/timeout/cap/profile/late result; uncancelable sync XHR must be resolved or documented without false claims |
| F01 | **Per-TV profiles + diagnostics** — after B01 | Sol; Astra review on isolation | Separate user preferences from device performance; installation UUID, migration, and local overrides. Same account on Android/Samsung keeps different budgets. Diagnostics displays cache effectively retained after retest/rollback; do not confuse 300 MB texture cache with process RAM |
| F02 | **Seekr** — engine already tested | Sol | Wire local states/provider/key/network/storage/clock into player and Settings; UTC day counter and localized quota reset display; Retry-After WGT; diagnostics. Cache/singleflight avoids duplicate queries; failure after dispatch consumes reserve; personal key does not reset 50/day |
| F03 | **Streams fitting connection bandwidth + speed test** — in progress | Sol/high | Reliable bytes per file and real duration; conservative budget for same host/network with origin/age. Stable partitioning within existing order/groups, preserved unknowns; stable snapshot/focus. Preserve first-source mode. Feed with real test and Android transfers; slow host does not imply slow Wi-Fi |
| F04 | **Simple subtitles + primary/secondary** — after B03/F01 | Sol | Initial screen: language + source (embedded/addon), only chosen languages; More options for details/versions. Independent selections/offsets and late arrival without focus loss; implement second overlay and collision detection |
| F05 | **Subtitle AutoSync** — depends on F04 documents/selector; second language adds second renderer | Sol for wiring; Astra for confidence/review | Complete embedded reference in parallel document without track switching; sessions and cancellation. Quick/Thorough, tolerance, undo, and secondary reference. Prove one language first, then two, without blocking primary pipeline on second renderer. Start movie without delay; apply only high-confidence result. Adversarial corpus: zero false positives observed; p95 residual error between accepted <= configured tolerance, with sample size, rejections, and limits reported. Proof per backend |
| F06 | **Audio/optional model Sync** — after PCM proof | Astra for contract; Sol for Android and collectors | Independent toggle; VAD and local English ASR evaluation. Explicit one-time download with size/hash/cancel/remove. Compare methods by latency/effectiveness/RAM. Passthrough/offload not silently disabled. LG/Samsung require demonstration of dialogue access or budgeted auxiliary decoding |
| F07 | **Disk seek buffer + volume up to 200%** — after F00/F01/N01 | Sol Android; Astra only for closed backends | Android session cache 256/512/1024 MB, Range/auth/cancel/ENOSPC/crash/cleanup. Gain per session, tint above 100%, and reset; audio compatibility. LG/Samsung: prove cache/gain on backend; AVPlay internal buffer does not prove temporary file or boost |
| F08 | **Social with unified identity** — after F01 | Astra contract; Sol client/backend | Independent Nuvio Social without Trakt/Simkl login requirement, canonical profile bound via verified identity, deduplicated friends/activities/channels. Movie/series/genre/taste/match comparisons with real coverage; explicit indicator for progress or private/absent data. Horizontal card, menu outside card, mark watched and reaction independent. Review backend and migration; deploy still pending |
| F09 | **Plugins — #134** — after N01/F01 | Astra short review; Sol implementation | Port modules from `plugins2` selectively. HTTP per call, bounds checking before allocation, global JS/DOM/queue budget, timeout/cancellation, account/profile generation, and cache. Empty snapshot/remove last repo with proper ACK. Glass UI; toolchains/licenses and tests per target |
| F10 | **P2P — #171** — after F00/F01/N01 | Astra short review; Sol implementation | Port `p2p2` selectively. Cancel without holding mutex during metadata/probe, discard outdated responses, hard disk/RAM cap, conservative statvfs failure handling, watchdog outside UI thread, cleanup only after halting writers. Package/test real engine on LG/Android/TPK; WGT lacks sockets for local engine |
| F11 | **Jellyfin and other personal servers** — after F00/F01/N01 | Sol; Astra profile/auth | Jellyfin first: connect/login/library/details/DirectPlay/progress. Then Emby and Plex with custom contracts. Isolated origin/IDs/users, device PlaybackInfo/profile, HLS/Range/auth, resume/transcode/check-ins, and session termination. Research completed, implementation unstarted |
| F12 | **TopN, icons, and scripts** | Luna for scripts/notes; Sol UI/Android | Verify actual TopN backlog and cost/pagination. Integrate optional icons without closing task when changing alias; address #226/#234. Verify Linux/Mac portability and space-padded paths. Legacy Settings experiments do not replace v2 |

Implementation references: [player](../../plans/player-1.8/README.md), [AutoSync engine](../../plans/player-1.8/AUTOSYNC-IMPLEMENTATION.md), [personal servers](../../plans/media-servers-1.8/README.md). Simkl and Letterboxd are not fully integrated channels today; access/API/contract must be verified before advertising coverage.

### Wave 3 — Consolidated Visuals and Interaction (Parallel to Waves 1/2)

Sol responsible, with Luna for translations and fixtures; use approved design and owner's latest fixes.

**Current direction, after starting execution:** owner rejected new details visuals and requested reverting to legacy layout. Use `v1.7.4` as visual reference for title page, preserving subsequent functional fixes, fan-out, navigation, and new integrations. Restoration replaces U02 and Glass details redesign proposal below. Do not restore entire file blindly without mapping functional differences; preserve snapshot beforehand. Consistency, veil, and performance tasks continue, now on top of restored visuals. Home, menus, and global material follow their own tasks.

- Apple TV: **logo -> direct buttons -> information**. Maintain concise, readable synopsis; eliminate giant paragraph and inline cast summary from hero. If artwork has confirmed foreign language, localized title appears in information; without logo, title takes its place. Cast section remains on page. Image sent by owner on 10/03 at 20:35 serves as content, density, and legibility reference; does not overturn button ordering or approved logo priority.
- Reading veil, enlarged text when scrolling down, lower placement on card and expanded details, carousel locked in expanded mode; fan-out spawns from Add on focus and clicking still adds. Restart enlarges on focus; duration/replay/remaining time and smaller seasons.
- Modern centered and 20% larger; Apple TV menu with approved margins/spacing. Settings 80/90/100%, default 90%. Validate navigation/drawing/measurement at same scale.
- Apple menu, Guide addon/category panels, sources, and send-to-friend respect shared material/opacity/Frost; verify glass/solid, contrast, light accent, and reduced motion.
- Guide: inspect individually left hero, search, pill-key help, veil, and panels mentioned in handoff; mark integrated/superseded/pending with screenshot and D-pad.
- Discord: preserve link/presence/pause/resume/profile switch/unlink; divergent inventory patch may be semantically redundant.
- Home: island persists during loading, displays useful metadata, and confirms completion for 1s. Measure provider latency separately from queue/parsing/art/render.
- Player: pause after seek, next episode blur, and Skip Credits overlap; audio/subtitles/error/loading expand from island per design. All detail entry points, including hero/filmography, use identical behavior.

Title/hero inconsistencies and veil clipping are explicit deliverables:

| ID | Task / Responsibility | Model | Deliverable / Completion Condition |
|---|---|---|---|
| U01 | **Consistent content between hero and details** — `home.c`, `detail.c`, and metadata helpers; coordinate hunks with B01/B02 | Sol/medium | Compare Modern and Apple TV across hero Home entries, expanded card/carousel, direct details, and filmography. Same available data and same actions, respecting geometry and material of each layout. Logo first, buttons below, metadata/country/director and short synopsis after; localized title only under approved conditions. Genres, duration, rating/score, and social data use consistent source and omit unknowns. Source quality/codec/count only show after obtaining reliable data, no mock entries. Test movie/series, localized/foreign/absent logo, absent/late synopsis, and long title; item does not lose synopsis or actions across entries/layouts |
| U02 | **Restore legacy details layout** — `detail.c/h`, reference `v1.7.4`; replaces Glass redesign | Sol/medium; coordinator reviews differences | Recover legacy composition and geometry of hero/details, buttons, synopsis, and background, comparing captures with tag. Preserve fan-out on focus, restart, enlarged text/carousel lock, duration/replay/remaining, back/filmography, removal of inline cast, and new integrations. Save patch of paused experiment. Separate visual changes from data/lifecycle fixes; verify movie/series, Modern/Apple, logo/long title, and episodes. Do not introduce novel layout arrangements during restoration. Host captures and physical D-pad are distinct forms of evidence |
| U03 | **Bottom line of Apple TV veil on downward scroll** — gradients, clipping, and hero->content transition compositing | Sol/medium | Reproduce reported horizontal boundary during expansion and scroll, comparing against identified older source/build when available. Check gradient termination, coverage outside card bounds, and section background; identify root cause only after reproducing. Continuous veil during transition without dividing line/box, preserving legibility and artwork. Test scroll down/up, card->expanded, glass/solid, Frost, and reduced motion; captures at intermediate states and TV photo/video with identified build |
| U04 | **Screen cost and smoothness** — before/after instrumentation of U01–U03, parallel to F00/F01 | Sol/high for renderer/concurrency; Luna for repeatable collection | Measure navigation/focus, expansion/scroll/fan-out, frame times, artwork upload/decode, cache, RAM, and Frost cost per platform. Reuse gradients/textures where applicable, invalidate cache with context/scale/material, and avoid repeated blur/decode on UI thread; apply only optimizations backed by measurements. No regressions in legibility, focus, latency, or memory leaks across cycles. See Wave 4 protocol; do not use static screenshot as proof of smoothness |

Historical diagnostics prior to `v1.7.4` restoration (do not blindly apply to restored layout): `heroWeb` in `detail.c` suppresses synopsis with `apple || serie`; `desenhaCopiaHero`/`desenhaHero` in `home.c` measure and draw text. Enabling synopsis also requires adjusting `heroTopo` and reserved height using identical text clipping/measurement. Candidate for U03 is `veuLeitura(-scrollY)`: movie mask terminates at `1080-scrollY`, and series combines gradient/background of different color. `gfx_veu_css` has strict spatial boundary; carousel uses distinct mask in `carFundo`. These are reproduction candidates, not proven physical causes. Test integer/fractional scroll, trailer on/off, and native/720p render; compare vertical pixel profile and frame times, avoiding indiscriminate fullscreen/FBO/blur passes.

U01–U04 form part of 1.8 closure. Content consistency does not require rendering all layouts with identical geometry. None of these tasks is complete simply because it was added to the plan.

Host `apple-no-cast-*` captures passed and demonstrate removal of cast line. These are fixtures, not TV photos or owner sign-off. Version 1.7.7/10707 installed previously was a local test label.

### Wave 4 — Optimize and Validate on Each Platform

Execute following functional integration on identical media/network fixtures. Baseline collected early in F00; in this wave repeat on final source. Compare 1.7.4 and 1.8 without feature versus 1.8 with isolated and full feature set; record hardware, firmware, commit, codec/HDR/audio/subtitles, source/host, and cold/warm cache. Ten iterations per scenario as initial protocol; measure median/p95/failures, without estimating numbers.

Cover three distinct latency metrics: **Home open -> useful rows**, **Click Play -> first frame with audio**, and **Remote command -> visual response**. For U01–U04 include Modern/Apple TV hero, expanded card, title screen, menus, fan-out, and veil scroll up/down. Record frame time distribution, dropped frames, decode/upload/cache, and memory per cycle; compare Frost/glass/solid and animations without masking provider delay inside render time. Diagnostics must report requested and effectively applied budget/cache, including 128/300 MB and rollback reasons.

| Platform | Measurement and Investigation | Dedicated Gates |
|---|---|---|
| **Android TV** | Click -> addons -> source -> URL -> prepare -> Playing -> first frame; Surface lifecycle, render/layout, Media3 buffers, transfers, cache/boost, heap/RSS/GC | Cold start on current hardware and Android 11/Mali-G31 scenario; preserve data/signature. Cache/boost/PCM per audio route; both ABIs and 16KB page compatibility when introducing new libraries |
| **LG webOS** | uMS/ACB, ROI/subtitles, art/decode, threads/texture cache, free disk, and P2P engine | Package cleanly installed via installer, mode 755, startup/player/seek/resume/HDR/ASS/CW. Test C9 and reported C4 scenario when available |
| **Samsung WGT** | AVPlay, WASM/IDBFS/localStorage, decode/Frost, menus, XHR calls, and track selection | Persistent quota/restart, tracks/trailers/subtitles, and reliable cancellation; no socket-based P2P engine; do not present TPK backends as WGT validation |
| **Samsung TPK 4/5 and 6+** | Prepare/Playing events, track settle, ROI, curl/jobs, textures/memory, and engine | TPK40 without compiler TLS and incompatible symbols; host/lifecycle calls separated from WGT; selection and fallback on target TV. Four package variants verified |

Optimize the specific stage measured as bottleneck: do not lower quality/HDR or increase buffers indiscriminately. Retest with same sample and rollback; manual choices persist. Require absence of reproducible regression in startup/seek/focus and absence of memory leaks across open/play/stop cycles. When hardware is unavailable, record as lacking physical proof and keep gate visible.

Proposed initial performance gate: beyond crashes/functional errors, block median or p95 regressions > 10% repeated across two comparable rounds exceeding measured baseline variance. Retain distribution/noise data; if inconclusive, retest without claiming performance gains. RAM/disk targets respect real profile/capability limits, and measurements cannot exceed hard ceiling. This protocol may be tuned with evidence prior to comparison, never to conceal results.

Focused per-module tests first. One serial integrated regression after freezing candidate, with documented exclusions and pre-existing failures. Do not re-run entire suite on every edit; new run only for relevant modification/failure/unresolved risk. ASan/UBSan/TSan on concurrent modules where appropriate. P2P test that SKIPs due to missing engine does not count as engine PASS.

### Wave 5 — Issues, Notes, Packages, and Shipping

Luna drafts notes/What's New, issue responses, and packaging. Sol verifies every statement matches code/tests. Coordinator integrates/publishes once delivery is ready and within authorized scope.

1. Update matrix `issue -> reproduced cause -> fix/commit -> test -> build -> physical result -> next step`. Respond in English with evidence and delivered version; review comments already posted on 1.7.4 to avoid repeating promises.
2. Close only verified, resolved scenarios. Unmet feature requests, thank-you notes, experimental ports, and issues without diagnostic data retain appropriate status. Do not bulk-close by ticket number.
3. Freeze a 1.8.0 commit; appinfo/config/Android/TPK and banners match. Create clean packaging source on external SSD; Mac had ~3.8GB free. Existing signatures and core account/addons/log are mandatory.
4. Generate 2 IPKs, APK, WGT, and 4 TPKs, cores required by updater, `repo.json`, `webosbrew.manifest.json`, and SHA256SUMS. Verify update filenames, ABIs/symbols/mode 755/certificates, absence of private credentials, and hashes.
5. Install this candidate on test targets, verify hash/version/banner, and run smoke/performance tests. Intermediate Android preview does not replace final package.
6. Finalize English release notes and 1.8 visual card showing only delivered features. Fresh install sees current card; 1.7.3/1.7.4 bookmark history preserved.
7. Review Social server/migration changes and backward compatibility with older clients before deploy; document backend used in tests. Private database/records are not release materials.
8. Publish tag/release/attachments and Homebrew metadata from same commit; download published attachments and verify SHA256. Verify update channel. Link verified fixes to issues and monitor concrete regressions.

## Open Issue Triage

GitHub query 10/03: 22 open issues; status subject to change. [#229](https://github.com/iqui27/nuvio-native-legacy/issues/229) is closed in current query; preserve rating tests and do not duplicate older response.

| Group | Issues | Next Steps |
|---|---|---|
| Immediate in-flight fixes | [#233](https://github.com/iqui27/nuvio-native-legacy/issues/233), [#228](https://github.com/iqui27/nuvio-native-legacy/issues/228), [#158](https://github.com/iqui27/nuvio-native-legacy/issues/158) | B01–B03; physical validation/recent logs before claiming issues resolved |
| Startup, player, and catalog | #223, #211, #188, #197 | B04; prioritize by reproducibility/severity, proper platform context |
| Fixed/configured in base, still open | #232, #231, #227, #209, #202, #216 | Revalidate blur, Cinemeta, language, contrast, and touch in candidate; reply without confusing existing setting with new fix |
| 1.8 Features | #134, #171, #226 | Plugins, P2P, and icons with respective gates |
| New request | #234 | Evaluate UX for icon/splash, trailer overlay, and manual button. Do not label partial configuration as complete resolution; overlay/splash require behavioral decision and validation |
| Observability / UI feedback | #192, #172, #144 | Separate diagnostics from real crash; map feedback to visual/physical flows |
| Experimental port | #135 | VIDAA in dedicated matrix; do not block Android/LG/Samsung or promise port merely because branch merged |
| Informational | #155 | Appreciation note, no technical fix required |

## Exit Checklist

- [ ] All useful work has explicit destination; owner checkout preserved.
- [ ] #158/#228/#233 integrated with regression and current result recorded; Flix without false attribution.
- [ ] Recent logs accessible or explicit limitation noted; candidate P0/P1 reproducible bugs resolved.
- [ ] Announced features connected end-to-end, with sandboxing/cancellation/budget/capability.
- [ ] Nuvio Social does not require connecting Trakt/Simkl and no field synthesizes mock comparison data.
- [ ] Approved layout preserved; real captures/builds identified per platform.
- [ ] Startup/seek/smoothness/memory measured; diagnostic retest and rollback verified.
- [ ] Version, signature, libraries, updater/Homebrew, and hashes checked.
- [ ] Notes/What's New/issues state only what was delivered and tested.
- [ ] Publishing and update checks verified after upload; limitations remain documented.

## Next Execution

Complete B01/#233 and F03/stream-fit hooks; consolidate tested B02/#228 and B03/#158. Luna Android preview is installed with dedicated manifest; log collection still needs unblocking. In parallel with functional integration, execute U01–U04: content/synopsis, veil continuity, and measured performance per platform. Next: Seekr UI/diagnostics + profiles, Social, subtitles/AutoSync, and Android cache/boost; plugins/P2P via reviewed modules; media servers/audio per contracts above. Freezing, benchmarks, and release follow functional integration, not merely compiling.
