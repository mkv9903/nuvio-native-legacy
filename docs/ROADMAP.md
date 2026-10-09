# General Roadmap for Nuvio Native Legacy

## Current Planning — 10/03/2026

1.7.4 is already published. The current plan to integrate, optimize and deliver 1.8 is in [releases/1.8.0/PLANO.md](releases/1.8.0/PLANO.md), with [Git inventory](releases/1.8.0/INVENTARIO.md). The stages/versions below are historical records from 10/02 and do not reflect current state.

Updated on 10/02/2026 21:39 BRT. Local planning on `release/1.7.2`, with compiled candidate from `b71471bc`. This is the version map; detailed audit of logs, branches and sessions is in [releases/1.7.2/ROADMAP.md](releases/1.7.2/ROADMAP.md).

The owner rejected the island mockup and requested continuity on the roadmap. The four features from that mockup are removed from the queue. The already integrated island and its account/profile isolation bugfix remain. There is no new approved island design.

**Authorized inclusion in 1.7.2:** LG screensaver fix `8a49956`, cherry-pick `857f83e3` + setting `11835ffa`. Focused test and i18n PASS; awaiting TV API confirmation and new packaging. The `b71471bc` packages mentioned below are the prior batch, without this inclusion.

## Version Distribution

| Target | Goal and Content | Status / Condition |
|---|---|---|
| **1.7.2** | Fixes, resilience, consistency, two-minute resume, functional Settings, touch/login, incremental sources and already implemented optimizations. Investigate current reports of startup, playback, catalog and aspect ratio before declaring scenarios resolved. | Integrated code and checked local packages; physical validation and triage of new reports still pending. Not a published release. |
| **1.7.3 — proposal** | Social/recommendations/reactions, Discord, configurable TopN and Linux script portability. Small player improvements can be selected by cost and compatibility. | Branches exist for Social/Discord; remaining items are not ready implementations. Select scope and review against stabilized base. No additional merge authorized by this planning alone. |
| **1.8 — plugins, P2P and Glass UI** | Integrate `plugins2` and `p2p2`, per-call cancellation and disk/memory budget; visual candidate `feat/glass-ilha`, with glass/solid surfaces and reorganization of Sources/menu/panels. | Candidate branches, not accepted features. Prove LG/Android/TPK; WGT does not receive socket-based P2P engine. |
| **Ports and exploration — uncommitted version** | VIDAA, webOS3 and NaCl/Tizen 4 experiments; visual/launcher ideas not yet selected. | Each port requires dedicated device/tester and matrix. Historical experiments are not 1.7.2 merge blockers. |

1.7.3 is a grouping recommendation, not a commitment to ship all candidates together. No fabricated dates. Proven fix preventing use of a supported platform remains 1.7.2 stabilization; do not automatically push bugs into feature releases.

## 1.7.2 — What Goes In

**New Settings are already in this version:** merge `f9083b10`, 184 options across 11 categories, search/advanced and reorganized navigation. Included in local packages from `b71471bc`. [Local native capture](releases/1.7.2/ajustes-tela-inicial.png) · [Validation and other captures](releases/1.7.2/VALIDACAO-LOCAL.md). Awaiting remote control / panel verification on TVs.

Already integrated and exercised by focused tests:

- **Functional Settings:** categories, search, advanced, and navigation; enum/defaults and source timeouts preserved. `agent/ajustesux` already merged.
- **Sources/resume:** incremental sources and selection timeout (#221), last source with fallback, Android receiving saved point in preparation and exact two-minute retention. `abrirrapido` includes `ilhavolta` and `i221`.
- **Account/offline:** cache/session preserved during outage; durable addon queue per account/profile, exact ACK and resume after restart. Remote HTTP 500 was not fixed client-side.
- **Consistency:** concurrent catalog, watched badge/history by identity, rejection of stale response, card/modal/request of island isolated between accounts/profiles.
- **Inputs and art:** touch/login (#216), embedded mobile icon for Samsung core update. 1.7.1 features, like mobile typing, translations and Seekr, are baseline; not counted as new merge.
- **LG Screensaver:** subscription on valid load; keep disabled during active playback and release on pause/end/error/stop. Owner request incorporated; physical proof pending.
- **Probe/build:** HTTP/headers/URL validation without truncation, bounded body; SSD temporaries and personal file exclusion. APK, two IPKs, WGT, four TPKs, two cores, metadata and checksums verified.

### Blockers for Calling 1.7.2 Ready

| Priority | Cases | Required Work |
|---|---|---|
| **P0: startup/login** | #224 LG 65UT7300LA/webOS 25 and #225 LG 50NANO75SPA (1.7.1); #223 TCL/Mali-G31/Android 11 (1.7.0); #211 UK6540 | **#224/#225, package cause:** the two published 1.7.1 `.ipk` files carry `nuvio-proto` with mode `-rwx---r--` inside `data.tar.gz` (in 1.7.0 it was `-rwxr-xr-x`). The app runs as uid 5152, without execute permission, and exits on launch. C9 did not report because binary was swapped with manual `chmod 755`. Root cause: `cp` over an existing `deploy/app/nuvio-proto` preserves old mode. Local 1.7.2 `.ipk` files (`nuvio-rel-172/final`) already ship `755`. `tools/arm.sh` now enforces `755` and aborts if package mode diverges. Awaiting `.ipk` install via installer on non-rooted TV. Users on 1.7.1 cannot launch app to update: reinstall via Homebrew Channel. **#223/#211:** no proven relation; #223 describes black screen with active loop, not crash. |
| **P1: playback** | Xtream #158; Android 3003/2004; Samsung video/audio #188; embedded ASS | Reproduce same source/headers/device; check effective version and distinguish diagnostics from regular usage. Xtream second-connection fix is already 1.7.1 baseline; confirm continuity. |
| **P1: catalog and aspect ratio** | #195/#197, history/badge, empty catalog and scroll selection | Test reporter's scenario/configuration. Fixed races do not prove all symptoms resolved. Do not reintroduce legacy window/ROI canaries. |
| **Existing UX validation** | #144 CW/borders/flicker, #202 contrast, #209 language, #216 touch, #221 timeout | Real device capture/control and matching scale/configuration. Reorganized Settings is not equivalent to resolving poster flicker. |
| **Per-device gates** | C9, Samsung TPK4/5 and 6+, WGT, TCL and Android touch | Retention 119/120 s, expired source, profile/logout, startup, HDR, subtitles and navigation. Measure speed on identical media; test fixtures do not prove physical performance gains. |

GitHub query for this update found **23 open issues**, compared to 20 in previous audit. #223/#224/#225 account for the difference. New reports in 1.7.1 do not prove regression in 1.7.2 candidate, but require triage before promising fixes.

Targeted query on Android logs 19066/19107/19164 was read-only (`rows_written=0`, `changed_db=false`). Confirmed: the six 3003 errors in 19066 belong to Live TV diagnostic attempts A/B/C; they are not six sessions or six movies. The other two records contain no detailed Java/HTTP cause in central payload. Root cause of 3003/2004 remains unproven. Private data remains outside Git.

## 1.7.3 — Next Increment Proposal

| Order | Feature / Issue | Concrete Origin | Acceptance Criteria |
|---|---|---|---|
| 1 | **Social, recommendations, reactions and friends activity** | `feat/social`; aggregates `agent/reacao`, `agent/socialsrv`, `agent/socialui` | Review a single aggregate branch against 1.7.2. Profile/account, compatibility with active server, network failure, lists/reactions and LG/Samsung/Android. Do not perform four redundant merges or redeploy old server. |
| 2 | **Discord Rich Presence — #222** | `feat/discord-presenca` | Link, publish presence, pause/resume, switch profile and unlink on real TV; review storage and OAuth contract. Mac protocol does not prove TV presence. |
| 3 | **Configurable TopN — #201** | Product request; first row already fixed | Choose limits/pagination and measure cost on legacy TV. Do not confuse corrected auto-count with new 10/12/18/24 option. |
| 4 | **Linux build — #217** | Four suggestions from reporter; no PR in previous audit | GNU/BSD sed, ffmpeg fallback, TPK subset and NV_WEBOS identity; test Linux/Mac and per-target preprocessing. Do not duplicate external contributions without verification. |
| 5 | **Minor player improvements — portion of #195 requests** | Subtitle/logo settings as product candidates | Individual scope, backend capability and owner visual review. Do not automatically bundle thumbnails/media server in this group. |
| 6 | **Triage/observability — #192** | Current aggregator mixes diagnostics, versions and auxiliary codes | Separate attempt/context/version/actual error; safe root causes without URL, credentials or private media title. Do not classify missing exit signal as proven crash. |

Launcher icons (`agent/icones`) and A/B alternatives (`agent/ajustesvisual`) remain without approved direction; they are not promised deliverables for 1.7.3. Rejected island mockup is not a dependency for these items.

## 1.8 — Plugins, P2P and Glass UI

| Area | Candidate | Scope and Criteria |
|---|---|---|
| **Glass UI / panels as islands** | `feat/glass-ilha` (`c5269ed4`) | 1.8 target indicated by owner. Port visual layer onto stabilized base, preserving functional Settings, #221 incremental sources, enum/defaults and identity. Glass/solid, D-pad/touch, contrast, reduced motion and GPU/memory budget on C9/TCL/Samsung. |
| **Plugins — #134** | `agent/plugins2` | Incremental integration without repeating #221; cancellable scraper, response timeout/limit, profile isolation and LG/Android/TPK/WGT matrix per capabilities. |
| **P2P — #171** | `agent/p2p2` | Engine and fallback on supported targets; real cancellation, bounded cache/disk, cleanup, memory and real TPK validation. WGT remains excluded from socket engine. |
| **Pre-caching and normal opening in parallel** | Future work, not demonstrated merge | Depends on per-call cancellation, GETRange/debrid behavior and budget. Last saved source already opens in parallel in 1.7.2. No guarantee of instant media playback. |
| **Media server/thumbnails — #195 requests** | Evaluation, no demonstrated code in this audit | Define contract, frame source, network/disk/GPU cost and cancellation. Can leverage this infrastructure, but not an automatic promise for 1.8. |

### Glass UI: Content and Integration Unit

On 10/02/2026 owner designated `feat/glass-ilha` for 1.8. Git check in this cycle: tip `c5269ed4`, with `feat/social` (`46e9b24f`) as ancestor. Contains **12 non-merge commits specific to Social**, modifying 59 files; broad diff against 1.7.2 also includes Social and does not represent visual layer alone.

Demonstrated commit contents:

- Sources sheet as island: resolution/HDR/SDR groupings, Cached/Dubbed filters, Best for this TV, tab navigation and content title.
- Menu/rail and Saved/Social panel as glass/solid surfaces; segmented tabs and surface-based focus.
- Poster menu, row style, Spotlight, Schedule, Library, Profile/stats in matching pattern.
- Associated art/icons, translations and capture fixtures.

**Dependency:** if Social enters 1.7.3, review only visual delta over that base later. If Social does not enter, separate/review the portion depending on Saved/Social before integration; do not pull Social/server automatically via branch merge. Do not replace current files with 1.7.1 tree copies: 1.7.2 has newer Settings, sync, partial sources, and isolation.

1.8 target is planning, without merge in this round. Requires owner visual review and TV testing; external report of Sources sheet on TCL does not validate full UI or LG/Samsung. This native work is distinct from rejected HTML clock island mockup; that mockup remains out of queue.

Do not use `agent/plugins` or `agent/p2p`: legacy versions superseded by candidates 2. Preserve commits; do not re-merge.

## Ports, Proposals, and History

| Group | Target |
|---|---|
| `feat/vidaa` / #135 | Independent stream until hardware proof and tester exist. Do not assign stable version beforehand. |
| `webos3` | Dedicated legacy compatibility; pick baseline/budget. Do not apply five patches across all platforms by default. |
| `feat/tizen4-coop`, `nacl/app-completo`, `spike/nacl-tizen45`, `worktree-agent-a03fb2e226f125a82` | Historical experiments; do not replace current TPK4/5. Preserve; review only if current limitations necessitate this path. |
| `agent/i195`, `i195b`, `i195c`, `i195d`, `i203`, `canary/tpk-audio-foco`, `canary/tpk-youtube-retoma` | Canaries prior to published fix. Do not constitute a future release merely because git cherry indicates patch+. Semantic comparison required before reusing snippets. |
| `agent/guiaunicode` | Fix already adapted to 1.7.1. No new merge/release. |
| `experiment/universo-2026` | Patch-level exploration; 36 patches do not equal 36 accepted features. No version assignment before separating current/duplicate/experimental content. |
| `worktree-agent-a53ac8d8d82f17106`, `worktree-agent-a62e3f13ac3f493e5`, `worktree-agent-af383a462c2f04823` | WIP/preservations; audit contents before assigning feature or deleting. |
| #172 fork ideas, `agent/ajustesvisual`, `agent/icones` | Future visual choices. Functional Settings already in 1.7.2. Rejected island mockup remains out of queue. |
| #190 resolved title/episode; #155 appreciation | Administrative, not new features. Public replies/closures require authorization. |

## Work Order and Publication Rules

1. Investigate new startup/login and pending 1.7.2 playback issues; preserve current candidate and checksums pending verified fix.
2. Complete short device matrix. New fix receives focused test and uniquely identified package; avoid unnecessary long test suites.
3. Owner selects 1.7.3 scope; review Social/Discord integration units separately and keep current external server.
4. Prepare 1.8 contracts and proofs for plugins/P2P; port/review Glass UI after resolving Social dependency and preserving 1.7.2 flows.
5. Publish/respond to issues only following validation and authorization. All planning remains local.

No pending merge for `abrirrapido`, `ilhavolta`, `i221`, `i216`, `ajustesux`, `offline`, `i212`, `i213a`, `i213b`, `quedas171`: integration verified in audit. Detailed mapping of the 29 refs and sessions is in audit doc; do not double-count aggregated Social branches or equivalent experiments.
