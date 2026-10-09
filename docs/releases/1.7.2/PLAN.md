# Approved Plan for 1.7.2 — 10/02/2026

Local only, in `release/1.7.2` (`/private/tmp/nv-172`), base 1.7.1 `2bc9659b`. Owner authorized execution of this proposal and chose functional reorganization of Settings; A/B skins, supporter icons, and social features remain excluded. Focused tests + i18n; TV behavior tested by owner. Zero push/tag/publishing/comment in this round.

## Subsequent Inclusion — webOS Screensaver

Explicit owner request: include `8a4995617bb4a43810a46e064dbfd7a61540146e` in 1.7.2. Local cherry-pick `857f83e3`; integration adjustment `11835ffa`. Original call was in LS2 registration failure path. Now subscribes in valid load callback, once per bus cycle, without blocking retries on transport failure.

Responds to Active state using received timestamp without truncation/rounding. Blocks screensaver strictly when media is actively playing; unblocks on pause, end, error, or absent media. Ignores non-Active states, missing/invalid/long timestamps, and post-shutdown callbacks. Reset allows subscription on subsequent backend cycle.

**Candidate status:** updated code; focused tests and i18n PASS. Physical validation and repackaging pending. Packages `b71471bc` precede screensaver. Details in LOCAL-VALIDATION.md.

## Already Integrated

`agente/abrirrapido` (includes `ilhavolta` and `i221`) and `agente/i216`. New island, partial sources, configurable add-on wait, last session source with fallback, touch/drag, and email login. Retention decided at two minutes, commit `1bf49696`; 119/120s test passed. Nine focused tests approved. In previous installation of this round, TCL received stable 1.7.1 APK; another session reports subsequent social APK install. Package currently on device not re-queried. 1.7.2 delivery and physical validation will have dedicated log.

Stored source verification already runs in parallel (`app.c:tocarFonteGuardada`); do not treat as future work in that path. Parallel verification of normal opening postponed in this round; probe was hardened, but per-call cancellation remains required before widening concurrency.

## Local Execution in this Round

- `2d6f2eb9`: catalog operations protected, late update checks title identity; history shifts on account/profile boundary and rejects stale Trakt/extras responses. Focused ASan/UBSan and ThreadSanitizer passed.
- `49ad2fbc`: mobile icon embedded in core, identical Lucide package artwork, single upload per context. Log 18898 proves 1.7.1 core loaded via memfd alongside host api11 and absent artwork; does not state exact old package version. Normal test passed; ASan attempt with SDL stalled before main and does not count as approval.
- `d8402e54`: durable add-on queue per account/profile; HTTP 500 followed by restart restores edits before network. Confirmation removes strictly exact snapshot; read/remove errors preserve retry and block stale pull. Logout purges snapshots and temporaries; 8 entries allocated on demand, zero disk access per frame. Sync order and offline account with ASan/UBSan and independent review passed. Write failure retains edit in RAM and logs issue; does not promise durability without working storage.
- `fc711f6b`: temporary templates respect chosen storage on Android/ARM/TPK/Samsung check; final exclusion and guard reach account snapshots and temp files. Final source `b71471bc` includes this fix across all platforms.
- `bff916b4`: add-on snapshot on main thread, review/ack per edit and profile; 500 failure retains local change, avoids applying pull preceding push, and does not lose edit made while push responds. Retry uses existing cadence/backoff. Sync order/offline/limits with ASan/UBSan passed. No server deployment; remote HTTP 500 cause not demonstrated.
- `f9083b10`: functional reorganization `ajustesux` merged by owner choice. Defaults and persisted enum preserved; add-on wait remains in Playback section, default 5 seconds. `61ab2263` proves four timeout options, local scope, single entry in Playback → Source Selection, and search results without duplication. Data, interaction, sections, defaults, profiles, and i18n passed; 12 native UX screenshots captured and reviewed on Mac. Bright focus with dark text, active category, and readable helper texts; simulated keys cover confirm/cancel/Back/restore/search/advanced. These proofs do not replace physical TV remotes and displays.
- `4412ae0c` / `1b74378d`: Android receives valid local position during preparation, with per-session ack and fallback; initial ack enters launch telemetry. Tests distinguish valid position, percentage only, episode/profile, live, and retained session; Kotlin/NDK compile. Prefetch timing corrected only in log.
- Generic parallel check of first media postponed: GET Range can create files/incur debrid quota, and current path lacks transfer cancellation. Stored resume already opens in parallel. Correct probe contract before expanding concurrency.
- `5fcae85c`: native probe validates HTTP 2xx, headers, transport, and URL without truncation; discards body and caps at ceiling. WGT avoids copying body to WASM heap and preserves inconclusive authorization when XHR cannot supply headers that AVPlay transmits. Native ASan/UBSan targets, WGT policy, three source regressions, TPK4/6 syntax, and i18n passed. Synchronous XHR does not yet provide per-call cancellation/timeout.
- TPK aspect ratio: source cropping disabled by default due to backend limitation; do not reintroduce off-screen ROI that previously exposed Samsung Home. No .NET alteration; #195 still depends on reporter scenario/hardware/source.

## Island Post-Finding

Owner-requested inventory confirmed incomplete isolation of resume card and UI/request copies during account/profile switch. Retained player already blocks stale identity; card/fallback could still expose previous title/episode. Fix `b71471bc` approved with ASan/UBSan and independent review: identity/instance validated before events, rendering, and fallback; invalidates card, modal, request, and flight animation, including post-idle. Notices/premieres and API outage sessions preserved. Packages `fc711f6b` retained as intermediate; fresh batch completed from this fix, with 13 attachments and checksums approved. Mockup and new features remain separate proposals.

## Execution and Pending Tasks

1. **Consistency — Completed in code.** `2d6f2eb9` guards reads/writes to published vector without recursive mutex and rejects updates whose index changed titles. Watched map now transitions on effective account/profile boundary; generation captured before network prevents delayed response from repopulating new identity. Same identity preserves local/other-source evidence. Tests covered A→B, profile, logout, A→B→A, progress fallback, 6,400 insertions across 8 threads, and concurrent catalog publication. ASan/UBSan and ThreadSanitizer approved; physical validation pending.
2. **TPK Auto-Update and Artwork — Fallback implemented.** Five 1.7.1 logs report missing `aj_smartphone.png`; all four full 1.7.1 packages contain this file. One log proves core updated via memfd, host api11, and missing artwork, without revealing exact older package version. `49ad2fbc` embeds essential icon in core and avoids missing-file attempts per frame. Verifying on Samsung with older host remains physical test; hypothesis was not generalized to all logs.
3. **Synchronization and Outages — Client protection completed.** Two Android logs show HTTP 500 add-on push; 521/522/429 also occur in Nuvio operations. `bff916b4` preserves local edits after failure, drops other-profile ack, keeps edits made during POST, and retains existing backoff. Durable queue in `d8402e54` adds cross-process restoration and protection against transient boot/ACK read errors. Focused tests with ASan/UBSan and independent review passed. Remote cause of 500 not demonstrated; zero servers altered or deployed. Do not claim verified external service recovery.
4. **Media Startup — Android and telemetry implemented; hardware gain pending.** Preparation receives valid position, with per-session confirmation and fallback without duplicate seek. Request→sources→URL→ready→first frame→saved position can be benchmarked on same source/hardware. 3–6s gain cited in handoff is prior estimate/measurement, not result of this implementation. Generic parallel verification of first media postponed due to debrid GET Range effects and lack of per-call cancellation. Stored last-session source already uses integrated parallel path.
5. **MKV Prefetch — Diagnostics corrected.** Log stops emitting false unsigned time when `now` precedes timestamp generated in same frame. Up to 4s wait remains; not reduced as part of telemetry fix. Checking embedded ASS subtitle in real media remains targeted physical test.
6. **Visual/Playback Reports — Hardware evidence pending.** Aspect ratio (#195), Continue Watching flicker (#144), missing catalogs (#195/#197), and localization (#209) require current scenario/source/config/log. Do not assume single root cause. #158 already received 1.7.1 reply; awaits reporter continuous playback confirmation, without new reply in this round.
7. **UX — Choice and merge completed.** Owner selected `ajustesux`; `f9083b10` integrated functional reorganization, preserving enum/defaults and #221. `ajustesvisual` A/B, supporter icons, and social features remain excluded from 1.7.2. Do not combine visual alternatives automatically or alter Android launcher without independent decision and proof.
8. **Local Packages Completed Post-Island Isolation; Hardware Pending.** Version 1.7.2 bumped locally in `32e70e48`. LG normal/highcache, WGT, four TPKs, two .so, dual ABI APK, and two Homebrew JSONs generated from `fc711f6b` core, with artwork/config/signature and 13 attachments/checksums verified. TPK tool received additional `06d915f6` check without altering runtime. Batch reclassified as intermediate after island finding. All packages rebuilt from identical `b71471bc` source, with three platform verifications and 12 checksums PASS; results and hashes in PACKAGES.md. Zero installs of this candidate executed. Owner tests C9, TCL, and Samsung: before/after two minutes, expired source, slow add-on, profile switch, login/touch, aspect ratio, and Xtream. Publishing remains subject to subsequent authorization.

## Log Evidence

Read-only update at 20:37 BRT on 10/02/2026: 118 logs labeled 1.7.1 (66 Android, 31 TPK, 14 webOS, 7 WGT); zero labeled 1.7.2. 30 logs with HTTP 5xx, 5 with missing icon, 41 without clean shutdown, and zero explicit fatal markers in aggregate. Counts are log entries, not sessions or proven crashes. The [roadmap](ROADMAP.md) cross-references this latest query with issues, branches, and sessions; earlier 65-log excerpt remains as audit history.

Read-only query of 65 available 1.7.1 logs: 40 Android, 11 TPK, 14 webOS; 64 distinct bodies. Does not equate to 64 independent sessions. 17 logs contain HTTP 5xx; 5 TPKs have missing icon. 36 contain unclean shutdown (27 Android, 9 LG), which does not prove crash. Zero explicit `FATAL EXCEPTION`, `Fatal signal`, `Segmentation fault`, `RuntimeError:`, or `Aborted` markers located in this slice. Do not declare full stability or regression from raw counts. Zero 1.7.2 logs used as validation of new build. Full subsequent snapshot at 20:39 BRT has 119 logs / 118 bodies, 67 being Android; details and new media errors are in ROADMAP.md.

Automated #192 reports confuse ASS fallback, exit code zero, and unclean shutdown with error/crash in some findings. Validate session, actual core version, and package/host version before prioritizing each event. Private data remains outside repository.

## Platform Optimization

- Android: initial Media3 position, video/HDR plane coherence on app switch, UI matched to TCL GPU, touch without duplicate firing; constrained memory and dual ABIs.
- LG: single pipeline, confirmed pause prior to retention, catalog/history race condition, Xtream proxy two-connection behavior; respect older TV capacities.
- TPK: updated resources alongside core, aspect ratio / single-window and app resume; ABI 4/5 without TLS and with DT_HASH. .NET host modifications require device evidence.
- WGT: partial sources / bounded cache, Chrome69 compatibility, non-blocking UI; do not attempt P2P without raw sockets.

## Loose Branches

Expanded query in this round found new refs and sessions beyond the 17 active branches in this initial inventory. Updated triage, including social/Discord and historical preservation, is in [roadmap](ROADMAP.md). Zero additional merges of these features authorized or executed.

Inventory reviewed via ancestry + `git cherry` against `32e70e48`: 17 branches with patches lacking exact equivalence. `agente/ajustesux` is integrated and has zero pending patches; listed below strictly to record completed status. Count does not imply every patch is functionally novel; adapted cherry-picks may appear unintegrated. Other worktrees were not altered or reevaluated for uncommitted changes.

| Branch | Patches Lacking Exact Equivalence | Status / Next Step |
|---|---:|---|
| `agente/ajustesux` | 0 | Integrated in `f9083b10`; focused validation and 12 screenshots approved on host |
| `agente/ajustesvisual` | 2 | A/B alternatives outside 1.7.2; functional direction `ajustesux` already selected |
| `agente/guiaunicode` | 1 | Unicode genre fix already present; do not revert newer Spotlight search |
| `agente/i195` | 1 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `agente/i195b` | 1 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `agente/i195c` | 2 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `agente/i195d` | 3 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `agente/i203` | 1 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `agente/icones` | 5 | Outside 1.7.2; requires Android launcher decision and proof |
| `agente/p2p` | 6 | Older version; do not merge |
| `agente/p2p2` | 13 | 1.8; requires platform / cancellation / disk validation |
| `agente/plugins` | 14 | Older version; do not merge |
| `agente/plugins2` | 21 | 1.8; requires platform / cancellation / disk validation |
| `agente/reacao` | 6 | Outside 1.7.2; separate session, preserve without merge in this round |
| `agente/socialsrv` | 7 | Outside 1.7.2; separate session, preserve without merge in this round |
| `agente/socialui` | 9 | Outside 1.7.2; advanced since initial inspection, preserve without merge |
| `feat/tizen4-coop` | 5 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |
| `feat/vidaa` | 7 | Older canary/port; preserve and confirm obsolescence, do not merge or delete now |

Socialsrv/socialui have 7/9 patches lacking exact equivalence in current revision; socialui had 6 in initial inspection. In initial inspection uncommitted changes also existed in socialui; worktree not reopened in this review. Do not assume stability from commit counts. Zero social branches merged in this round.

## Consulted Open Issues

Read-only GitHub re-query after Settings merge: same 20 open issues, with matching numbers and timestamps to initial snapshot; zero new scope blockers identified. Initial inspection found zero open PRs; PRs not re-queried in this review. #212/#213/#215 were already closed and #158 replied in 1.7.1 by another session. Do not duplicate replies.

| Issue | Status / Triage |
|---|---|
| [#222](https://github.com/iqui27/nuvio-native-legacy/issues/222) | Discord RP request: assess companion app feasibility/dependencies; outside TV bug block |
| [#221](https://github.com/iqui27/nuvio-native-legacy/issues/221) | Integrated: add-ons arrive incrementally; Settings merge preserves 5s timeout and search. Owner confirmation on Samsung pending |
| [#217](https://github.com/iqui27/nuvio-native-legacy/issues/217) | Linux portability: sed, artwork converter, package selection, and NV_WEBOS; zero PR at initial inspection, coordinate prior to duplicating contribution |
| [#216](https://github.com/iqui27/nuvio-native-legacy/issues/216) | Integrated: touch/drag/login; owner validation on physical device pending |
| [#211](https://github.com/iqui27/nuvio-native-legacy/issues/211) | UK6540 startup: no log/firmware; awaiting evidence, do not declare resolved |
| [#209](https://github.com/iqui27/nuvio-native-legacy/issues/209) | Published fix; confirm behavior/localization in current scenario |
| [#202](https://github.com/iqui27/nuvio-native-legacy/issues/202) | Published fix; 12 screenshots of new UX reviewed on host, dark text on bright focus. Check physical TV panel/remote |
| [#201](https://github.com/iqui27/nuvio-native-legacy/issues/201) | User confirmed first row; request for more TopN items is separate product decision |
| [#197](https://github.com/iqui27/nuvio-native-legacy/issues/197) | Catalog depends on add-on / profile / empty return; request current scenario/log if persisting |
| [#195](https://github.com/iqui27/nuvio-native-legacy/issues/195) | Trailer confirmed fixed; new report includes aspect ratio and catalogs, alongside feature requests |
| [#192](https://github.com/iqui27/nuvio-native-legacy/issues/192) | Aggregator: review raw events, do not treat automated labels as diagnosis |
| [#190](https://github.com/iqui27/nuvio-native-legacy/issues/190) | User confirmed resolution; administrative task pending |
| [#188](https://github.com/iqui27/nuvio-native-legacy/issues/188) | Prior fix; confirm same scenario in current TPK |
| [#172](https://github.com/iqui27/nuvio-native-legacy/issues/172) | Functional Settings reorganization integrated; remaining visual requests not assumed resolved. Measure smoothness on TVs |
| [#171](https://github.com/iqui27/nuvio-native-legacy/issues/171) | Native P2P reserved for 1.8 |
| [#158](https://github.com/iqui27/nuvio-native-legacy/issues/158) | 1.7.1 reply already sent; await continuous playback confirmation from reporter |
| [#155](https://github.com/iqui27/nuvio-native-legacy/issues/155) | Appreciation note, zero bugs to resolve |
| [#144](https://github.com/iqui27/nuvio-native-legacy/issues/144) | UX parity and Continue Watching flicker report: reproduce |
| [#135](https://github.com/iqui27/nuvio-native-legacy/issues/135) | Experimental VIDAA, excluded from 1.7.2 |
| [#134](https://github.com/iqui27/nuvio-native-legacy/issues/134) | Plugins reserved for 1.8 |

## Scope Completion Criteria

Data from other profile never displayed; TPK update does not depend on nonexistent artwork; Nuvio failures preserve data and queue; resume and fallback operate correctly; video format and subtitles do not regress. Measure per-stage timing improvements on identical source and device, without promising instant playback on new media. Modified code tests + i18n and short owner tests; do not rerun long test suite.
