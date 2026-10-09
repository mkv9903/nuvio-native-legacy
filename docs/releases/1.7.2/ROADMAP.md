# Release 1.7.2 Audit: Logs, Issues, and External Session Work

**Version Cross-Reference:** [General Roadmap — 1.7.2, 1.7.3, and 1.8](../../ROADMAP.md). This file preserves the detailed release candidate audit.

## Subsequent Update — 10/02/2026 21:39 BRT

The owner rejected the island mockup; the four proposed evolutions are excluded from the queue and are not release dependencies. The existing island and integrated identity bugfix remain. A fresh GitHub query identified 23 open issues: #223 (Android/TCL black screen, 1.7.0 report), #224 (LG 65UT7300LA / webOS 25, 1.7.1 startup failure), and #225 (LG 50NANO75SPA, post-update 1.7.1 failure) were added to startup/login triage. #224/#225 include zero logs. Do not infer a single root cause or fix from package verification. Future destinations are explicit in the general roadmap; 1.7.3 is a proposal, plugins/P2P in 1.8 follow previously chosen direction.

Review of 10/02/2026, with evidence gathered between 20:34 and 20:48 BRT and final package status updated in this round. Local work in `release/1.7.2`, `/private/tmp/nv-172`, atop published 1.7.1 (`2bc9659b`). Initial audited source: `fc711f6b`; new candidate post-island isolation: `b71471bc20e36806de9665145e4898c5e8e67884`. Scope may receive subsequent owner decisions; this document recommends next steps and authorizes zero new merges or releases.

The priority is completing the approved candidate, validating real scenarios, and resolving reproduced bugs. Closing all issues is not a quality gate: there are product requests, thank-you notes, portability patches, and reports still awaiting data. Integrated code, generated packages, and observed TV behavior constitute distinct evidence.

Local references: [approved plan](PLAN.md), [focused validation and TV test cases](LOCAL-VALIDATION.md), [packages and build provenance](PACKAGES.md), [draft notes](NOTES.md). The external 1.7.1 handoff was reviewed; subsequent decisions on two-minute retention and functional Settings supersede earlier alternatives.

## Post-Island Analysis Update

The [island inventory and mockup](ISLAND-PROPOSAL.md) revealed a P2 defect confirmed via callchain: card, modal/request, and animated assets were not all bound to account/profile. Retained session already blocked previous identity; card fallback could use stale title/episode in new context. Focused fix `b71471bc` approved with ASan/UBSan and independent review, without adding mockup proposals. Also invalidates modal/request/artwork/flight and rejects stale instances even when new profile accesses identical episode; repackaging completed: three platforms PASS and 12 checksums approved. The 13-attachment batch `fc711f6b` passed technical checks but is retained as intermediate post-fix; new candidate derives from `b71471bc`. Prior audit below preserves timestamps and evidence.

## Subsequent Screensaver Inclusion

`8a49956` authorized by owner for 1.7.2. Integrated in `857f83e3`, activation corrected in `11835ffa`; focused test and i18n PASS. API lacks physical confirmation; repackaging pending. Audited batch `b71471bc` below does not include this fix. Details in LOCAL-VALIDATION.md.

## Confirmed State in this Review

- GitHub read-only query at 20:34 BRT: **20 open issues and zero open PRs**. Latest comments on #158, #195, #201, #217, #222, #144, #192, and #190 reviewed. #212/#213/#215 were already addressed in 1.7.1; do not duplicate replies.
- D1 read-only query at 20:37 BRT: **118 logs labeled 1.7.1**. Second capture at 20:39 BRT contained **119 logs and 118 distinct bodies**. These do not represent 118 or 119 independent sessions/devices. Zero logs labeled 1.7.2 in either query.
- Broad Git inventory: **29 refs with at least one patch lacking exact equivalence**, including **17 branches from previous inventory**, two recent features, and ten historical/experimental refs. These groups overlap in commits; do not sum patches as fresh work.
- `abrirrapido`/`ilhavolta`/`i221`, `i216`, and `ajustesux` are already in release. Durable add-on queue (`d8402e54`) and explicit temporaries on SSD (`fc711f6b`) closed in this round; queue review and focused tests passed. Batch from that source preserved as intermediate. Current 13 attachments passed generation, check, and consolidated checksums from `b71471bc`, per PACKAGES.md. Tooling update `06d915f6` complements standalone TPK guard, consuming entire listing to prevent SIGPIPE; 12k-entry fixture and final ZIP verification passed. Does not alter runtime; included in source of new candidate.
- Zero push, tag, release, issue comment, server deploy, or TV installation executed by this audit.

## Logs: What Changed and What Remains Unproven

Previous 65-log excerpt remains valid as earlier audit; table below describes 20:39 BRT capture. Raw data kept private outside Git. D1 queries returned `rows_written=0`, `changes=0`, and `changed_db=false`.

| Labeled Platform | 1.7.1 Logs | Logs with HTTP 5xx | Missing Mobile Icon | Unclean Exit | Explicit Fatal Marker |
|---|---:|---:|---:|---:|---:|
| Android | 67 | 11 | 0 | 31 | 0 |
| Samsung WGT (`tizen`) | 7 | 6 | 0 | 0 | 0 |
| Samsung TPK | 31 | 11 | 5 | 1 | 0 |
| LG webOS | 14 | 2 | 0 | 9 | 0 |
| **Total** | **119** | **30** | **5** | **41** | **0** |

A single log may contain multiple events and retries. Absence of `FATAL EXCEPTION`, `Fatal signal`, `Segmentation fault`, `RuntimeError:`, or `Aborted` does not prove stability; unclean exit does not prove crash. Not all HTTP 5xx errors originate from Nuvio: on new network lines route/origin is not always identified. Do not attribute all 30 logs to `api.nuvio.tv` or sync.

| Evidence | Interpretation & Next Step |
|---|---|
| 13 additional logs post-earlier excerpt contain HTTP 5xx; 502/504 on WGT/TPK and 503/504 on Android | Maintains resilience, backoff, and error reporting priorities. Distinguish account, add-ons, artwork, trailers, and provider before fixing a route. Client protection does not prove remote service recovery. |
| 5 TPK logs with missing `aj_smartphone.png`; zero additions post-earlier excerpt | Built-in fallback already integrated. Log 18898 demonstrates 1.7.1 core via memfd, api11 host, and missing artwork, but does not provide exact older package version. Confirm core update on older host on device. |
| Android 19066: 6 player 3003 errors; 19107: 2004 and 3003; 19164: 3003 | Real playback failures to investigate, not crash markers. First two have `[tv]` 1.7.1 marker; 19164 body lacks version banner. Distinguish Live TV diagnostic attempts from movie playback and verify source/Content-Type/headers/fallback. Do not assume three separate users. |
| Excerpt 16626–16630, received 06:49 UTC, reported local 09:49, 1.7.0 core | Scenario matches #158: HLS fetches chunks, proxy shuts down in 1–2s; subsequent connection ends without chunks. TV reaches `loadCompleted`/`currentTime`/`endOfStream`. Corresponds to second connection mechanism resolved in 1.7.1. Awaiting continuous playback confirmation on current version. Probable timestamp/scenario match; body does not explicitly identify C4 model. |
| LG log search today for UK6540 yielded zero matches | #211 remains without identified bootstrap log or firmware. Absence in excerpt does not prove hardware functions or that failure did not occur. |

In official [Media3 PlaybackException](https://developer.android.com/reference/androidx/media3/common/PlaybackException) reference, 3003 maps to unsupported container/feature; 2004 maps to unexpected HTTP status. Classifies failure, does not diagnose app root cause. Companion zero in events is bridge auxiliary argument; does not turn 3003 into success. Reproduce with authorized sample and verify core version, package, bridge, and target prior to changing parser or player.

Aggregator [#192](https://github.com/iqui27/nuvio-native-legacy/issues/192) conflates versions, diagnostic events, ASS fallbacks, auxiliary codes, and shutdowns. Regression/crash tags are hypotheses; roadmap relies on raw events and reporter confirmations.

## Priorities to Complete Candidate

P0 blocks candidate acceptance; P1 requires investigation or proof before declaring scenario resolved; P2/P3 covers product, maintenance, or future platforms. P1 lacking reproduction does not warrant speculative merge.

| Priority | Origin: Issue/Log/Session/Branch | Evidence / Hypothesis | Merge Decision | Dependency & Validation |
|---|---|---|---|---|
| P0 | Offline queue; current session; `d8402e54` | Add-on edits following push 500 previously survived in RAM only; needed persistence across restart | **Yes now: integrated**, do not repeat merge | ASan/UBSan and review passed. Final packages must exclude snapshots and personal temporaries. Account/profile A→B→A, exact ACK, edit during POST, disk failure, logout. |
| P0 | Local build; `fc711f6b` | macOS template-less `mktemp` extracted to internal drive despite SSD TMPDIR | **Yes now: integrated** | Generate all 13 attachments from identical source; version, signature, ABI, config, hashes, and zero personal files. Intermediate candidate `32e70e48` invalid as final. |
| P0 | Owner: C9, Samsung, and TCL | Host test and package do not prove TV renderer/player/remote | **Requires physical hardware verification**, zero assumed patches | Matrix below; record source/build/device/firmware/timing. Do not publish while owner cases remain pending. |
| P1 | [#158](https://github.com/iqui27/nuvio-native-legacy/issues/158), 09:49 logs | Pause after seconds, proxy second connection, 1.7.1 reply already sent | **Already integrated in 1.7.1**; new patch requires current reproduction | C9 continuous live TV, channel switch, diagnostics; confirmation on reporter's LG C4 pending. Do not close based on C9 test alone. |
| P1 | [#221](https://github.com/iqui27/nuvio-native-legacy/issues/221), `i221` in `abrirrapido` | Samsung Tizen 5 waited 20–25s for sources; incremental delivery integrated | **Already integrated** | Samsung TPK 4/5 and 6+, fast+slow add-ons, autoplay, and 5s default timeout. WGT: UI responsiveness during query. |
| P1 | Return to media / island, `abrirrapido`/`ilhavolta`, Android initial position | Retention 119/120s and preparation with saved point pass fixtures; real gain unmeasured | **Already integrated**, retain 2 minutes | C9/TCL/Samsung before and after 120s; expired source, fallback, episode/profile, HDR, app switch. Compare milestones on same media/device. |
| P1 | New Android errors 19066/19107/19164 | 3003/2004 during media attempts; cause unproven | **Requires reproduction**, do not merge alternative player | Classify diagnostic/live/VOD, identify media/headers/HTTP without exposing personal URLs, check container, message, and fallback on TCL. |
| P1 | Catalog/history; [#197](https://github.com/iqui27/nuvio-native-legacy/issues/197), part of [#195](https://github.com/iqui27/nuvio-native-legacy/issues/195) | Race conditions/identity resolved; empty catalog, scroll autoplay, and settings may have distinct causes | **Consistency fix integrated**; remainder depends on scenario | Same account/profile/add-on and reporter config; 7 rows, scroll without selecting; profile switch during response. Focused ASan/TSan passed. |
| P1 | [#195](https://github.com/iqui27/nuvio-native-legacy/issues/195), aspect ratio | Reporter confirmed trailer resolved, now reports aspect ratio in 95% of cases | **Requires proof**, do not reintroduce old canaries | Samsung TPK 6.5+/4/5 with same media and each mode. Off-screen ROI previously exposed Home; source crop remains backend-limited. |
| P1 | [#188](https://github.com/iqui27/nuvio-native-legacy/issues/188) | Audio with Home behind player on older TPK; single-window fix published | **Already integrated**; patch only with current log/build | Samsung S90D / reporter scenario; trailer → movie, background resume, video/GL plane. |
| P1 | [#211](https://github.com/iqui27/nuvio-native-legacy/issues/211) | UK6540 returns to input on startup; unknown version/firmware/log | **Requires data**, do not mark resolved | Bootstrap log, installed IPK version, firmware, and target; compare normal/highcache if relevant. C9 does not substitute UK6540. |
| P1 | Embedded ASS / probe / HTTP; logs and current client | Subtitle fallback does not imply video failure; probe may incur network/quota side effects | **Probe hardening integrated**; generic parallel check postponed | MKV with embedded ASS on C9, TPK, and TCL. HTTP 403 with headers, redirect, timeout, and expired source; WGT preserves AVPlay decision when XHR cannot send header. |
| P2 | [#144](https://github.com/iqui27/nuvio-native-legacy/issues/144) | Cut bottom corners and Continue Watching flicker; Settings are separate point in issue | **Settings integrated**; flicker requires reproduction | Same posters/layout/scale; navigate vertically to CW, capture frame and TV GPU profile. Do not treat Settings redesign as poster fix. |
| Out of Queue | Island mockup; agent `island_ux_mockup` | Owner rejected mockup and directed continuity into roadmap | **Do not integrate four proposed evolutions** | Preserve artifact as history. Existing island and identity fix remain; not a new visual design. |
| P2 | [#216](https://github.com/iqui27/nuvio-native-legacy/issues/216), `i216` | Touch/profiles and manual login, previously unsupported | **Already integrated** | Android phone/tablet for touch and IME; TCL for D-pad/login regression. Pointer simulation on Mac does not prove physical touch. |
| P2 | [#202](https://github.com/iqui27/nuvio-native-legacy/issues/202) and [#209](https://github.com/iqui27/nuvio-native-legacy/issues/209) | Contrast and localization published; functional UX had 12 host screenshots approved | **Already integrated/published**, confirm scenario | C9: accent matching without white-on-white; series description/title in user language, TMDB+add-on and correct profile. |
| P2 | [#217](https://github.com/iqui27/nuvio-native-legacy/issues/217) | Four Linux host improvements; author invited to submit PRs, none open | **Depends on contribution and review** | Do not duplicate patch set. GNU/BSD sed, ffmpeg fallback, TPK subset, NV_WEBOS: Linux/Mac builds, per-target preprocessing, C9, and TPK api11. No deploy required. |

## Product and Administrative Issues

| Issue | Action / Next Step | Timing |
|---|---|---|
| [#222 Discord](https://github.com/iqui27/nuvio-native-legacy/issues/222) | New implementation exists in `feat/discord-presenca`; initial infeasibility diagnosis superseded, but hardware proof and OAuth contract review pending. Zero merges authorized in this round. | Later release or explicit scope expansion, following platform matrix and per-profile unlinking. |
| [#201 TopN](https://github.com/iqui27/nuvio-native-legacy/issues/201) | Reporter confirmed first row resolved. More Top 10/12/18/24 items is layout/product decision, separate from bug. | Define limit/pagination and cost on legacy TVs before implementation. |
| [#195 requests](https://github.com/iqui27/nuvio-native-legacy/issues/195#issuecomment-5960258625) | Seekbar thumbnails, title logo, subtitle offset, and media servers require dedicated triage; do not bundle with aspect ratio fix. | Proposal with cost/compatibility and per-feature validation. Plugins remain in 1.8. |
| [#172 visual ideas](https://github.com/iqui27/nuvio-native-legacy/issues/172) | Functional Settings chosen and integrated; other fork ideas not assumed approved. | Selective flow/readability review, based on current captures and GPU profiling. |
| [#171 P2P](https://github.com/iqui27/nuvio-native-legacy/issues/171) | Use `agente/p2p2` as future candidate, not legacy branch. Disk, cancellation, and real TPK limit acceptance. Socket-less WGT excluded. | **1.8**, decision already finalized. |
| [#134 plugins](https://github.com/iqui27/nuvio-native-legacy/issues/134) | Use `agente/plugins2`, not legacy version. C9 has prior measurements, but real TPK/WGT and frozen scrapers pending validation. | **1.8**, with per-call cancellation, memory/time limits, and profiles. |
| [#135 VIDAA](https://github.com/iqui27/nuvio-native-legacy/issues/135) | Experimental port without hardware proof; do not extrapolate desktop Chrome findings. | Independent track with tester/device/Chromium version. |
| [#190](https://github.com/iqui27/nuvio-native-legacy/issues/190#issuecomment-5956133326) | Reporter confirmed title/episode resolved. Issue administration remains, zero new patches. | Only with reply/closure authorization. |
| [#155](https://github.com/iqui27/nuvio-native-legacy/issues/155) | Thank-you note; zero defects to resolve. | Zero release dependency. |
| [#192](https://github.com/iqui27/nuvio-native-legacy/issues/192) | Improve triage criteria; separate user/session/attempt/diagnostic/real version before filing new bugs. | Observability maintenance; zero postings in this round. |

## Sessions and Integration Unit

Codex titles below preserved as returned by tool. Chat state and listing timestamp do not prove a new patch was created: Git relationship was verified. Social/Discord sessions did not appear as identifiable chats in queried listing; external M3 logs and commits allow inventorying work without inventing titles or contacting authors.

| Session / Work | Observed State & Evidence | Integration Unit | Recommendation |
|---|---|---|---|
| **Optimize and fix code** — current chat | Final source `b71471bc`; queue `d8402e54` and island isolation closed, 13 final attachments approved; `queue_finish`/`queue_review` agents belong to this round | Local release commits; final package derives from frozen source | Packages completed; physical gates remain. Do not create synthetic merge for work done directly in release. |
| **Review docs/ajustes-ux.md** | Last turn confirms jade badge / white check in `2fbcf53b`; worktree clean at 20:39; branch is ancestor of release | `agente/ajustesux`, already merged in `f9083b10` | Do not merge again or treat badge as standalone patch. Other chat proposals require dedicated commit/proof. |
| **Polish UI to professional level** | Returned turns are historical: report UI/sources/What's New submitted in `fc2025a`; listing showed `notLoaded` and recent update | Historical commits, already in base; zero new patches demonstrated | Avoid duplication. Recent timestamp in listing does not make historical proposal a pending feature. |
| External Social session, unnamed | `feat/social` at `46e9b24f`, commit 20:00 BRT, clean worktree; aggregates `socialsrv`+`reacao`+`socialui` | **`feat/social` is aggregator**; 23 new patches, including 22 from the three branches and glue | Excluded from 1.7.2 per current decision. If approved later, review/merge aggregator once, preserving newer release. |
| External Discord session, unnamed | `feat/discord-presenca` at `6b6bfe14`, commit 19:48 BRT, clean worktree; 1 patch atop 1.7.1 | Discord patch/branch, with C/network/build/settings changes and per-profile OAuth files | Outside current scope; depends on integration review and hardware proof. Do not overwrite release with old tree or copy foreign session package as 1.7.2. |

**Coexistence with Server and Other Sessions' TVs.** Read-only `wrangler deployments list` confirmed Worker `nuvio-recomendacoes` version `23314845-8ba9-4471-83ca-1e6c713db44f` at 100%, published 20:06 BRT. Matches external Social log. Current release source still contains prior server: indiscriminate deploy would remove new routes. Zero deployment needed for 1.7.2 packages; none executed.

External M3 log reports Social APK installed on TCL and owner approval; install not audited here. Package on TCL not verified as pure stable APK. Proven in this round: **zero 1.7.2 builds from this release installed or validated on TCL**. For future tests, verify actual package/core/host before evaluating results. External Discord report mentions Android install and pre-rebase C9 build, without verified presence on TV; does not equate to candidate validation.

## Subsequent Destination of Glass UI

Owner indicated `feat/glass-ilha` for 1.8. Head `c5269ed4`, based on `feat/social` (`46e9b24f`), with 12 non-merge visual commits. Includes Sources, menu/rail, Saved/Social, menus, Spotlight, Schedule, Library, and Profile/stats; not just clock. No merge into 1.7.2. Review visual delta post-Social, preserve newer flows, and validate per platform. Content/dependencies in [general roadmap](../../ROADMAP.md). Rejected HTML mockup remains excluded. Addition to historical inventory, not retroactive update of 29 count.

## Inventory of Loose Branches

Method: ancestry and `git cherry` against release source. `+` indicates patch lacking exact equivalence; `-` indicates patch equivalence, even with different hash. Adapted patch may appear as `+` despite semantically present fix. Clean worktree indicates only absence of uncommitted edits at read time. Does not indicate testing or merge approval.

### The 17 Branches from Prior Inventory

| Branch | Patches `+` | Decision & Rationale |
|---|---:|---|
| `agente/ajustesvisual` | 2 | **Not now**: A/B excluded; functional Settings selected. |
| `agente/guiaunicode` | 1 | **No**: Unicode genre fixed by adaptation in 1.7.1; do not revert newer reader/search. |
| `agente/i195` | 1 | **Not now**: Old window canary; single-window published. Preserve until obsolescence verified. |
| `agente/i195b` | 1 | Same triage; do not combine old window strategies. |
| `agente/i195c` | 2 | Same triage; depended on specific host/API and canary. |
| `agente/i195d` | 3 | Same triage; canary 6 precedes final fix. |
| `agente/i203` | 1 | Same triage; legacy host, zero new reproduced defects. |
| `agente/icones` | 5 | **Different version/decision**: activity-alias affects Android launcher; requires dedicated acceptance and proof. |
| `agente/p2p` | 6 | **No**: superseded by `p2p2`; preserve, do not merge. |
| `agente/p2p2` | 13 | **1.8**: disk/cancellation/real TPK; WGT cannot support socket motor. |
| `agente/plugins` | 14 | **No**: superseded by `plugins2`. |
| `agente/plugins2` | 21 | **1.8**: shares #221; do not duplicate incremental sources. Prove TPK/WGT and cancellation. |
| `agente/reacao` | 6 | **Different version/decision**: ancestor of `feat/social`; avoid duplicate isolated merge. |
| `agente/socialsrv` | 7 | **Different version/decision**: ancestor of `feat/social`; external server advanced, separate deploy. |
| `agente/socialui` | 9 | **Different version/decision**: ancestor of `feat/social`; worktree clean. LG/Samsung unproven on new Social. |
| `feat/tizen4-coop` | 5 | **Not now**: historical port/canary; current TPK 4/5 package has distinct contract. |
| `feat/vidaa` | 7 | **Independent track**: requires real TV; do not block 1.7.2 stable platforms. |

### Two Features Added Since Prior Inventory

| Branch | Patches `+` | Decision |
|---|---:|---|
| `feat/social` | 23 | **Requires scope expansion**; aggregates three branches above, not 23 patches beyond them. Older client / newer server must coexist; do not redeploy previous server. |
| `feat/discord-presenca` | 1 | **Requires scope expansion / review**. Mac had protocol proof per commit; TV unproven for linking, presence, resume, and unlinking. |

### Ten Historical Refs / Experiments / Preservations

| Ref | Patches `+` | Next Step |
|---|---:|---|
| `canario/tpk-audio-foco` | 1 | Preserve; older canary with zero merge requests. |
| `canary/tpk-youtube-retoma` | 1 | Preserve; do not reintroduce old audio/resource-conflict logic without reproduction. |
| `experimento/universo-2026` | 36 | Future per-patch review. Mixes data exploration, UX, and old race conditions; avoid broad merge. |
| `nacl/app-completo` | 2 | Legacy experimental track; do not replace current TPK 4/5. |
| `spike/nacl-tizen45` | 1 | Shares NaCl experiment; do not count as independent implementation. |
| `webos3` | 5 | Retains one equivalent `-` patch; legacy target requires targeted review, not broad merge. |
| `worktree-agent-a03fb2e226f125a82` | 2 | Same content as `nacl/app-completo`; preservation, not fresh feature. |
| `worktree-agent-a53ac8d8d82f17106` | 1 | Legacy agent WIP; unaudited content, do not merge or delete. |
| `worktree-agent-a62e3f13ac3f493e5` | 1 | Legacy agent WIP; unaudited content, do not merge or delete. |
| `worktree-agent-af383a462c2f04823` | 1 | One equivalent `-` patch; old catalog/limit fix requires semantic comparison before use. |

Refs `abrirrapido`, `ilhavolta`, `i221`, `i216`, `ajustesux`, `offline`, `i212`, `i213a`, `i213b`, and `quedas171` have integration verified via ancestry/patch; excluded from merge queue. Worktree entries marked `prunable` indicate missing checkouts, not prune or commit deletion authorization. Zero external session worktrees altered.

## Recommended Sequence and Dependencies

1. **Authorized Local Candidate Completed.** Durable queue, scripts, and island isolation reside in `b71471bc`; generation, check, and checksums for 13 attachments passed with identical source hash. Use this batch for hardware round. Packages `32e70e48` and `fc711f6b` segregated as intermediate.
2. **Owner Conducts Short TV Testing Round.** Verify package/core/host prior to test; one person/agent per TV. Observe two boundaries, 119/120s, expired source, profile, Settings, live TV, and subtitles. Record failures with milestone and timestamp, without running full test suite.
3. **Investigate Pending P1s per Scenario.** Android 3003/2004, Samsung aspect ratio / catalogs, LG startup UK6540, and CW flicker should receive targeted reproductions. Modify solely demonstrated root causes; rerun area-specific tests and rebuild affected package. Avoid combining disparate hypotheses into broad patches.
4. **Reevaluate Scope with Owner.** Social and Discord possess substantial new work, but remain excluded from this candidate. Island mockup was rejected and dropped from queue. If selected, review each unit atop current release, preserving enums/defaults/identity/generations and retesting affected matrix. Aggregator Social avoids three redundant merges. Do not touch active server as side effect.
5. **Prepare 1.8.** Plugins and P2P demand per-call cancellation, disk/memory bounds, and backend proof. Generic parallel first-source open also depends on cancellable probe and debrid GET Range effect analysis; stored source already features parallel path.
6. **Publishing and Replies Subsequent to Hardware Pass.** After physical results and owner authorization, use notes and platform file table, publish verified attachments, and reply/administer genuinely resolved issues. Zero release authorization inferred from roadmap request.

## Short Per-Platform Matrix

| Platform | Required Cases | Acceptance / Limits |
|---|---|---|
| **LG C9 — normal/highcache IPK** | Island / resume 119/120s; expired source; continuous Xtream live TV and channel switch; MKV with **embedded ASS track**; profile/logout during sync; contrast/localization | Zero new load during valid retention; post-retention pipeline freed and valid fallback; live TV without second connection cut; device-compatible memory. C9 does not certify UK6540 or C4. |
| **Samsung WGT** | Incremental sources, UI during slow add-on / timeout, HTTP 403 with AVPlay headers, Settings/search/Back, profile/fallback | Chrome69, heap budget, non-blocking navigation. Synchronous XHR still carries timeout/cancellation limits; do not make unfounded guarantees. |
| **Samsung TPK 4/5** | #221 on Tizen 5; core via memfd, update on legacy host / icon, aspect ratio / modes, app resume, and trailer → movie | ABI without TLS/PT_TLS/TLS relocations, DT_HASH present, and essential artwork available. Build does not prove TV loading; 4/5 host requires matching hardware generation. |
| **Samsung TPK 6+ / 6.5 / api11** | Single-window plane, aspect ratio #195, audio without video #188, background resume, source/header, Settings, and icon | Distinguish host/API from core. Do not reintroduce ROI revealing Home; backend may not support all crops. Modern Samsung does not certify all APIs. |
| **Android/TCL — Dual ABI APK** | Saved point accepted in prep / fallback, live TV without seek, HDR / app switch, retention, expired source, 3003/2004, profiles | Benchmark request→sources→URL→ready→first frame→saved position on same media; zero duplicate seeks and no stale profile responses. Confirm present package post foreign builds. |
| **Android Phone/Tablet** | Touch, drag, profile selection, email login, keyboard, size/orientation | #216 requires physical touch. TCL D-pad does not substitute. |

Zero latency/FPS/memory measurements of new candidate on TV in this audit. Exploiting hardware requires backend-specific budgets and measurements; generic precache is not inherently safe and does not make new media playback instant.

## Expected Outcome of Next Decision

1.7.2 candidate with verifiable source and attachments, essential test cases conducted by owner, remaining bugs isolated per scenario, and feature roadmap separated from stabilization. Social/Discord enter only post explicit decision and proof; plugins/P2P remain in 1.8. Zero issue counts or missing fatal markers will be used to promise 100% resolution.
