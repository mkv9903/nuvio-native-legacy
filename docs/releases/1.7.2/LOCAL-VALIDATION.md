# Local Integration for 1.7.2 — 10/02/2026

Base: `origin/master` at `2bc9659b` (1.7.1). Worktree: `/private/tmp/nv-172`, branch `release/1.7.2`.

`agente/abrirrapido` integrated in `971788b2`; already contains `agente/ilhavolta` and `agente/i221`. `agente/i216` integrated in `4fa3647e`. No conflicts and no push, tag, release, or issue comments.

## Focused Verification

Passed: `i18n.sh`, `fonteauto.sh`, `fontecache_vod.sh`, `fontevolta.sh`, `ilha_voo.sh`, `ponteiro_toque.sh`, `sessao_email.sh`, `fontes_parcial.sh`, `player_retido.sh`. Local logs in `/private/tmp/nv172-<test>.log`. Host tests only, not hardware TV verification.

TCL received stable 1.7.1 APK from `~/.cache/nuvio-rel-171`, SHA256 verified, `-r` install successful and launch confirmed via active process. Android reported `versionCode=10701`, `versionName=1.7.1`. That install preceded 1.7.2; a later session reported a social APK and current package on TCL was not re-queried. Launch logcat excerpt contains no crash markers; does not prove playback or interaction.

## Pending Decisions and Tests

- Owner decided on two-minute retention. During retention the single pipeline prevents Home trailers; after deadline, stored source remains available for resume.
- Manifests identified as 1.7.2 in local packages. Scope remains open; no tag or publishing authorized.
- Owner tests behavior on TVs; do not run long test suite or control hardware devices without coordination and exclusive lock.
- MKV prefetch: only formatted print string corrected. Original wait time preserved.
- Android prepares with valid local position and fallback, tested in fixtures/JNI/Kotlin. Physical device gain not yet measured. Parallel normal opening check postponed due to debrid probe effects.
- Catalog and history fixed: locks on access, identity on updates, generation per account/profile/credential. Focused ASan/UBSan and ThreadSanitizer concurrency tests passed.
- Plugins/P2P deferred to 1.8. Functional Settings integrated per owner choice; A/B themes, supporter icons, and social remain out.
- Issue replies require owner authorization.

## Functional Settings

Merge `f9083b10`; regression #221 in `61ab2263`. 184 options, 11 categories. Passed: data, interaction, sections, defaults, profiles, and i18n (zero failures). Confirmation, cancellation, Back, restore, search, and advanced exercised with simulated keys; not physical remote. Add-on wait preserves 5 seconds, 4 choices, local scope, single input, and search.

12 native screenshots in `/private/tmp/nv172-ajustes-ux-review`. Samples from this integration:

![Settings initial screen](ajustes-tela-inicial.png)

[Selector](ajustes-seletor.png) · [Trailers](ajustes-trailers.png)

Panel artwork is illustrative, not active playback. Local review confirmed distinct focus, dark text on bright focus, and readable helper text. Cadence and TV GPU memory remain pending.

## Consistency and Return to Media

Sync order, offline account, and offline limits under ASan/UBSan: PASS. Push 500 does not erase local edits; stale ack does not lose new edits; 60s retry without bursts; does not apply pull preceding push. Profile/account snapshot isolated. No remote diagnostic or deployment for remote HTTP 500.

Android: numeric/JNI rule with ASan/UBSan, normal player integration and Kotlin/NDK PASS. Fixture accepted 612.345s in preparation with zero seeks; rejected bridge triggered one seek; percentage alone does not invent duration; retained session does not reload; live does not seek; episodes/profiles isolated. Log marker distinguishes accepted point in preparation. Zero physical speed measurements in this round.

TPK icon: identical embedded PNG, lazy load, 600 frames with single upload per context, cleanup/reinit/GL error PASS. TPK syntax and i18n PASS. ASan on this GL fixture stalled before main; result unavailable, not approval. .NET host unchanged.

## Owner TV Test Cases

1. LG C9: minimize/return before 120s without reloading; after 120s free pipeline / resume source; Xtream for sufficient duration to reproduce pause in #158; embedded ASS subtitles in MKV.
2. TCL: resume at saved point (compare URL→first-frame and ack markers), expired source with fallback, live without seek, touch/login, app switch / HDR.
3. Samsung: incremental sources #221 and configured wait, Settings/search/Back on remote, icon in full package and auto-update, aspect ratio of identical source #195. Distinguish unsupported backend crop modes.
4. All: switch profile and logout while history/sources arrive; watched badge correctness; Continue Watching #144 without flicker; catalog/localization under reporter's scenario #195/#197/#209.

None of these cases were confirmed on physical hardware in this round. Last install on TCL was stable 1.7.1 APK; roadmap notes subsequent report of social APK installed by another session. Device package was not re-queried and no 1.7.2 from this round was installed.

## Source Probe

`5fcae85c`: tests HTTP 500, 403 with Referer, 206, redirect, partial body, URL > 2000 / truncation, timeout, and server ignoring Range. Normal and ASan/UBSan PASS; native body discarded with limit, without downloading full media to probe. Final URL not yet reused for playback; do not claim it eliminates second redirect. WGT/EMJS policy PASS, no body copy to WASM heap. Synchronous XHR retains cancellation/timeout limits and may receive server body if Range ignored. AVPlay still decides 401/403 when add-on requires forbidden browser headers. Autoplay/cache/return and TPK4/6 syntax PASS.

## Durable Queue and Local Final Package

`d8402e54`: sync order and offline account with ASan/UBSan passed. Test uses separate processes for HTTP 500 → terminate → reopen, restore prior to network, and exact ACK. Covers empty name, disabled add-on, accounts A→B→A, profiles, edits during POST, previous profile ACK, write/read/remove errors, boot transient read, all-identities logout, and 8 RAM entries. Saved snapshots can leave RAM and reload from disk; failed-write snapshot stays in RAM. No disk queue access per frame. Independent review confirmed boot and ACK boundary read fixes. Log: `/Volumes/ExternalSSD/nv172-syncordem-fila.log`.

`fc711f6b`: package exclusions/guards cover `conta-*.txt` and temporaries; explicit templates force mktemp to respect TMPDIR. Shell syntax, guard checks, and SSD path regression passed. Was common commit of verified batch, later reclassified as intermediate due to island identity finding. Per-package results and checksums in [PACKAGES.md](PACKAGES.md); physical validation pending.

## Island Activity Isolation

`b71471bc20e36806de9665145e4898c5e8e67884`: callchain-confirmed bug fixed in VOD card and UI copies. Per-account/profile validation before events/updates, including login and profile selection; per-instance guard before fallback. Identity switch clears card, pill, modal, request, artwork, and flight animation. Owner remains trackable after idle card; same session during outage preserves resume. Existing notices, premiere, and timeouts preserved.

Clean fixture using real island/card state with mock catalog/identity/timer: `TMPDIR=/Volumes/ExternalSSD/nv172-build-tmp SANITIZE=1 bash tests/ilhacart_identidade.sh` returned zero and `tudo ok`. Covers profile/account switch, logout/relogin, identical IMDb/episode, stale instance, modal/request/flight, 30 minutes, and notice/premiere preservation. App/island syntax and diffcheck PASS; independent review clean. Log: `/Volumes/ExternalSSD/nv172-ilhacart-identidade.log`. Not a physical TV validation. New common batch uses this hash: Android/LG/Samsung checks PASS, 13 local attachments, 12 checksums approved. Zero installations or publishing. Reports in PACKAGES.md; mockup is independent proposal in ISLAND-PROPOSAL.md.

## Subsequent Inclusion — webOS Screensaver

Explicit owner request: include `8a4995617bb4a43810a46e064dbfd7a61540146e` in 1.7.2. Local cherry-pick `857f83e3`; integration adjustment `11835ffa`. Original call was in LS2 registration failure path. Now subscribes in valid load callback, once per bus cycle, without blocking retries on transport failure.

Responds to Active state using received timestamp without truncation/rounding. Blocks screensaver strictly when media is actively playing; unblocks on pause, end, error, or absent media. Ignores non-Active states, missing/invalid/long timestamps, and post-shutdown callbacks. Reset allows subscription on subsequent backend cycle.

`TMPDIR=/Volumes/ExternalSSD/nv172-build-tmp bash tests/video_pausa_lg.sh`: PASS, exercising real LS2 code with mock calls/payloads. Includes real load flow, no duplicate subscriptions, numeric/string timestamp, requested/pipeline pause, end, error, stop, call failure, and shutdown. Log `/Volumes/ExternalSSD/nv172-screensaver-focused.log`. ASan run did not complete and was stopped; does not count as approval or regression diagnosis. Log `/Volumes/ExternalSSD/nv172-screensaver-before.log`.

I18n PASS in `/Volumes/ExternalSSD/nv172-screensaver-i18n-fixed.log`. First scan flagged filename prefix `conta-addons-pend-`, preceding screensaver. `ab281e99` specifically classifies prefix as filename, without altering text/translations or offline queue.

**Pending:** Undocumented API lacks physical proof. Owner must test playback for >20 min without remote interaction, pause/resume, end/stop, and retained/paused film in island. Do not link this change to startup crashes without evidence. The `b71471bc` package batch remains verified as earlier artifact; does not include this fix. Repackaging candidate pending post-freeze.
