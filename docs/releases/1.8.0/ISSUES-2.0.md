# Open Issues vs. 2.0 Integration

Audit from 10/05/2026. Code: `codex/integration-180-glass` @ `09124343` (local, unpushed). Comparison base: tag `v1.7.4` (last release). 44 open issues on GitHub `iqui27/nuvio-native-legacy`. Read-only: nothing edited, nothing commented, no TV tests.

## Summary Count

| Category | Qty | Issues |
|---|---|---|
| RESOLVED IN 2.0 | 11 | #223, #231, #233, #235, #237, #238, #239, #243, #244, #245, #249 |
| RESOLVED EARLIER | 5 | #188, #197, #202, #209, #227 |
| PARTIAL | 12 | #144, #158, #172, #226, #234, #241, #246, #247, #252, #255, #256, #261 |
| ONLY IN OTHER BRANCH | 0 | — |
| UNADDRESSED | 5 | #250, #253, #254, #258, #260 |
| REQUIRES TV PROOF / REPORTER INFO | 6 | #134, #171, #211, #228, #232, #240 |
| NOT A BUG / OUT OF SCOPE | 5 | #135, #155, #192, #242, #257 |
| **Total** | **44** | |

## Release Blockers (Assessment)

No crash or data-loss issues are unaddressed in code. What remains critical is lack of on-TV validation:

1. **#223 Android TV, black screen on login/QR.** Fix (embedded CA, curl error handling, half-session discarded) is at HEAD, but reporter only tested up to 1.7.4. Test 2.0 APK on Android 11 before publishing.
2. **#158 LG C4, Live TV and Continue Watching.** TS-only accounts fail to decode on C4 (report from 10/01). Addon subtitles and offline CW (aad1c947) only exist in integration. Run on C9/C4 before claiming resolution.
3. **#233, #254, #255 Samsung .tpk, account catalogs and collections/Home.** #233 has substantial fix (3b625f58) without on-TV validation. #254 (ghost rows when removing addon) has no fix.
4. **#211 LG UK6540PSB, closes immediately on launch.** Launch crash without log. Cannot guarantee 2.0 launches on webOS 4. Request log again.
5. **Arabic on Samsung (#253, #258).** Not a crash, but 7 Arabic issues are open and Samsung lacks Arabic font: shaping/RTL landed, font did not. Bundling an Arabic font subset has the highest payoff.

Non-blockers: #134, #171 (new features without on-TV proof; ship with caveats), #228, #232, #241 (Samsung, depend on reporter testing).

## Outside Integration (Branches)

- `agente/icones` (5 commits: e2e54440, e7499919, 4aa68d99, 40f62fcb, e6010365): app icon gallery behind `apoiador_ativo()`. Helps #226 and #234 (icon item). Merge likely straightforward in icon files; Android uses activity-alias and requires testing.
- `feat/vidaa` (8 commits): VIDAA port from #135. Not part of package; remains separate.
- `perf/trim-fileira` (b4b007f2, 1 commit): onTrimMemory on Android, performance; does not cite open issue.
- `agente/i195*`, `agente/i203`, `fix/tpk-selo-hdr`: already have equivalent in releases <= 1.7.4 (1.6.3 etc.) or address already closed issues. No open issue depends on them.
- `agente/p2p`, `agente/plugins`: older lines; 2.0 uses f10-p2p and f09-plugins.

## Table

Legend: evidence = commit and file:line at HEAD `09124343`. "<= 1.7.4" = already in tag v1.7.4.

| # | Title | Platform | Category | Evidence | What is Missing |
|---|---|---|---|---|---|
| #134 | Please support plugin | LG webOS | REQUIRES TV PROOF / REPORTER INFO | f09-plugins (merge d5e84282): src/pluginjs.c (QuickJS), src/plugins.c, src/pluginsui.c, src/htmlq.c; docs/releases/1.8.0/F09-PLUGINS.md | No on-TV test or real scraper test (reporter's All-in-One repo). .wgt and Tizen 4/5 paths uncompiled. Assumes backend `plugins` table sync. |
| #135 | Hisense VIDAA: testers wanted | VIDAA | NOT A BUG / OUT OF SCOPE | Hosted port (Worker /tv/), not release package. Code only in feat/vidaa (8 commits outside integration, e.g. c1ddedb5, 70b27fd5). | Nothing to do in 2.0. Do not promise VIDAA in notes. Reporter stalled at TV developer mode. |
| #144 | Align UI with nuvioTV (Android) | LG webOS | PARTIAL | 1.5.x–1.6.x: b7d10412 (veil corners on Continue Watching), 98160e65 (stylized font fallback to plain). 2.0: full Glass UI (merges feat/glass-ilha, ui/w*). | Inherently open-ended request. Cropped corners and flicker in Continue Watching (09/30) without confirmation of resolution. No on-TV proof. |
| #155 | Thank you for building this | — | NOT A BUG / OUT OF SCOPE | Appreciation. | Can be closed. |
| #158 | Live TV Xtream: channels do not play, empty guide (C4) | LG webOS | PARTIAL | Crash opening channel: 1.5.2. Provider EPG: 1.5.3. A/B/C/D modes and TS proxy: 31a3da42, 5656c662, 22057178 (all <= 1.7.4). Addon subtitles and local offline CW: aad1c947 (integration only; src/addons.c, src/descoberta.c, tests/cwlocal.c). | New TS-only account: data arrives but C4 does not decode (10/01 report). Flix-Streams with no results and CW not showing: unconfirmed (docs/issues/158 does not prove their case). Needs new log on 2.0. |
| #171 | P2P/Torrentio addons missing from sources | Samsung .tpk 6+ | REQUIRES TV PROOF / REPORTER INFO | Owner response: excluded on purpose without debrid/P2P. 2.0: f10-p2p (775f9db9, src/p2pmotor.c, src/p2p.c); local debrid src/debrid.c. | Embedded P2P compiles in .tpk 6+ but never ran on physical Samsung TV (F10-P2P.md). Question answered; only proof matters. |
| #172 | UI: ideas from Corby7 fork | LG webOS | PARTIAL | Full text display instead of word-by-word: a65080ae, 414cd61e (1.6.x). 2.0: Glass UI. | Rest is design direction without completion criteria. Reporter only tested on C3. |
| #188 | Video does not show, only audio (S90D) | Samsung .tpk 6+ | RESOLVED EARLIER | 1.6.3: fee4e632, 359937e6, 5b0e4957 (tpk host, video window). Present in HEAD inherited from v1.7.4. | Reporter did not confirm after 1.6.3. Wait or close due to inactivity. |
| #192 | Log report | — | NOT A BUG / OUT OF SCOPE | Automated triage issue; not a bug. | Keep or pin outside bug list. |
| #197 | Catalogs do not appear (3 rows) | Samsung .tpk 5.5 | RESOLVED EARLIER | 1.6.4: b98ae904 and f5813122 (single row clearing). Owner confirmed via whoareux3 logs. | praveencudz: Bharat Binge disabled in profile (configuration, not bug). Scrolling selecting by itself: no response. |
| #202 | Matching highlight color hides text | LG webOS | RESOLVED EARLIER | 1.7.0: c522e1f2 (src/ajustes.c:95-103, contrast coloring). 2.0 reinforces: b6e14d69. | Reporter did not reply. Can be closed. |
| #209 | Show title/synopsis in English | LG webOS | RESOLVED EARLIER | 1.7.0: 0da72c2a (TMDB in other language overrides addon text). In v1.7.0 and v1.7.1. | In 2.0 catalog catalog.nuvio.tv became primary source (ecf852ea): review with TMDB in pt-BR. No new test covering it. |
| #211 | LG UK6540PSB crashes on launch | LG webOS (2018, webOS 4) | REQUIRES TV PROOF / REPORTER INFO | Nothing in code. Owner requested webOS version and /tmp/nuvio.log; no response. | Startup crash without data. Unknown if 2.0 runs on this TV. Request log again. |
| #223 | Android TV: black screen after 'Preparing QR' (TCL, Android 11) | Android TV | RESOLVED IN 2.0 | 665646f6 / merge 5ca3ec85: curl error on login screen, CA embedded in every request (src/rede.c:990,1711,2011; main.c:862 + deploy/app/art/discord-ca.pem), half-session discarded (src/sessao.c:588). Android 1.7.2: 105e759b. | Reporter confirmed 60 FPS on 1.7.4 and TLS failure. Reporter still needs to test 2.0. 2s wait in pedirSuperficie (NuvioActivity.kt:213) remains (root cause unproven). |
| #226 | Legacy / alternative icon | All | PARTIAL | 54316088 (merge de97342e): 'App Logo' New\|Classic and 'App Launch' in Settings (src/logoapp.c). Neutral splash: 1b832a97. | Launcher icon does not change. Icon gallery only in agente/icones (5 commits outside; Android uses activity-alias; behind apoiador_ativo()). Merge seems straightforward, but is Android and has supporter gate. |
| #227 | Localized title somewhere | All | RESOLVED EARLIER | 1.7.4: 6d74a0ac, f1903d6c. Still in HEAD: src/detail.c:3713 and 3827 (nomeAbaixo/hCaption). | Can be closed after reporter confirms. |
| #228 | Automatic trailer does not play (hero and poster) | Samsung .tpk 6+ (Q80A) | REQUIRES TV PROOF / REPORTER INFO | 1e03fc30 (fallback order and native trailer state; hero with retained row index); src/home.c, src/trailer.c, src/trailerfonte.c; docs/issues/228-home-trailers.md | Reporter still fails on 1.7.4. Doc itself states it does not prove their cause. Needs test on Q80A with 2.0 and log. |
| #231 | Option to turn off Cinemeta | Samsung/LG | RESOLVED IN 2.0 | d6495798: 'Search on Cinemeta' option (src/ajustes.c:1636, src/descoberta.c:739). | Covers search (what reporter requested). Does not disable Cinemeta in title details. |
| #232 | Next episode thumbnail not blurred | Unspecified | REQUIRES TV PROOF / REPORTER INFO | Post-play popup: 2495e39e (1.7.4, src/posplay.c:389,459). Home hero/cards: d6495798 (src/home.c:785,820,859). | Reporter still sees issue on 1.7.4 and provided log (10/04). Platform unknown and whether 'Episode thumbnail' is enabled. |
| #233 | Nuvio account does not sync catalogs/ordering | Samsung .tpk 6+ | RESOLVED IN 2.0 | 1.7.4 (64-entry limit); 3b625f58: stable IDs, order/hidden, editor matching Home (src/colecoes.c, src/fileiras.c, src/colfileiras.c; tests/colfileiras_sync.c). | Without proof on reporter's Samsung. Report from 10/03 (stale items in 'Outside Home') is what 3b625f58 addresses. |
| #234 | Icon/splash, trailer title (OLED), trailer button | LG webOS | PARTIAL | Neutral splash 1b832a97; Classic logo 54316088; title disappears during trailer with OLED 3fc5a37b (src/home.c:3211); fade 973a3780; 'Watch trailer' button 652b759d (src/detail.c:2042). | Launcher icon does not change (see #226; branch agente/icones). |
| #235 | Seek applied immediately | All | RESOLVED IN 2.0 | 652b759d: 1s window with Seekr (src/salto.h:85-88, src/player.c:2682). Without Seekr remains 420 ms. | Only applies with Seekr thumbnails enabled, which was reporter's context. |
| #237 | Missing '/' in channel input (Stalker/Xtream) | LG/Samsung/Android | RESOLVED IN 2.0 | e5a848c3, 8f8be06a (merge b1088476): src/ajustes.c:829 (alphabet with / and _), src/stalker.c (path prefix). | No on-TV proof; host tests (tests/stalker_portal.c). |
| #238 | 'Wait for add-ons' locked by depth effect | All | RESOLVED IN 2.0 | 73f93686 and d6495798: src/ajustes.c:4419 (AJ_FONTE_PRAZO without dependency). | — |
| #239 | Arabic: disconnected letters | LG webOS (C1) | RESOLVED IN 2.0 | 6bb186ac (merge d00870f0): src/bidi.c, src/text.c:751 (txt_bidi_legenda), src/player.c:3135, src/legendasui.c:708. LG system font: src/text.c:1147. | No on-TV proof. libass/ASS paths do not pass through bidi (embedded ASS subtitles remain unshaped). |
| #240 | Delay loading title (Torbox+addons) | LG webOS | REQUIRES TV PROOF / REPORTER INFO | Perf 2.0: perf/b2-arranque, ed27533e (instant title open), b3-texturas. Nothing specific from report. | No log, no metrics. 'Allowed addons' (filter per addon) does not exist. Request log. |
| #241 | Black bars in trailer returned | Samsung .tpk 6+ (AU7000) | PARTIAL | 50ba9279, 9a0cce0f (merge 50df3f42): Settings > Trailers > Zoom (experimental); src/ajustes.c:363,4825; src/video_tpk.c:491. | Default is Off: without enabling, bars persist. Help text warns about black screen on some Samsung TVs. No proof on AU7000. |
| #242 | Question: does TV Torbox/Debrid key work in addons? | All | NOT A BUG / OUT OF SCOPE | Yes for streams with infoHash only: src/debrid.c (account key or typed on TV). | Answer and close. |
| #243 | Missing IMDb score and '14' in Continue Watching | Samsung .tpk 5.0 | RESOLVED IN 2.0 | 8e7f4098 (removes invented '14'), ccfe01a8 (Cinemeta once per work): src/trakt.c, src/simkl.c, tests/trakt_cw_dup.c. | No on-TV proof. Reporter has own forks (fix/cw-*) to compare. |
| #244 | Continue Watching: same episode 5x and 'watched' does not remove | Samsung .tpk 5.0 | RESOLVED IN 2.0 | 8e7f4098 + d6495798 (merge 67065f88, 8d1acd4b): src/trakt.c:1256,1398 (dedupObras), src/ctxmenu.c:723, tests/cwdup244.c, tests/trakt_cw_dup.c. | CW source switch rebuilding row is 'unproven hypothesis on device' (commit message). |
| #245 | Arabic: loose letters and wrong order | LG webOS | RESOLVED IN 2.0 | Same fix as #239 (6bb186ac). | No on-TV proof. |
| #246 | HEVC anime: seek does not skip | webOS (probable) | PARTIAL | Diagnostics only: 023497dd (merge 394f9b7a), src/video.c:136,1805. | No fix. New log only valid after reporter runs 2.0. Platform not reported. |
| #247 | Arabic subtitles (encoding) | LG webOS | PARTIAL | Shaping/RTL: 6bb186ac. Decoding: src/legenda.c:495-560 only handles UTF-8/16, 1251, and 1252. | Windows-1256 (Arabic) not detected: subtitles in this codepage render incorrectly even with bidi. |
| #249 | Keep video full with 'Next' card | All | RESOLVED IN 2.0 | dee639f7 (merge a202c686): src/posplay.c:89 (posplay_sobre_video), src/player.c:2662. Series only; movie (related) still shrinks video. | Comment in posplay.c:411 says '52%' and is stale. No option to toggle; default is already full video. |
| #250 | Arabic interface language | All | UNADDRESSED | 29 `idioma_*.h` tables, none 'ar' (`ls src/idioma_*.h`). | New feature: text table + Arabic font + RTL. Out of scope for 2.0 unless requested by owner. |
| #252 | Default audio language for anime | LG webOS | PARTIAL | Global 'Audio Language' already chooses track per title: src/ajustes.c:850, src/video.c:876. | No anime-specific option exists. Tracks without language tags are ignored (video.c:884), common in anime. No proof. |
| #253 | Arabic renders as squares (UI and subtitles) | Samsung .tpk 5 | UNADDRESSED | src/text.c:1147-1152: Arabic fonts only on LG (/usr/share/fonts/DroidNaskh...), macOS, and Android. deploy/app/fonts lacks Arabic font. | Bundle Arabic font subset (like DroidSansFallback-Subset). Arabic UI and RTL do not exist (#250, #260). Without this Samsung remains square glyphs. |
| #254 | Home does not update when installing/removing addon (ghosts) | Samsung .tpk 6 | UNADDRESSED | No commit addresses addon removal. Related and unproven: 3b625f58 (collections), f5813122 (cleanup, 1.6.4). | Repro: install/remove addon with catalogs and collections. No log from reporter. |
| #255 | Home shows only 5 collections | Samsung .tpk 6 | PARTIAL | 3b625f58: collections do not consume catalog quota; src/home.c, src/fileiras.c. 'Row limit' exists (src/ajustes.c:876) and cut-off counts collections (home.c:2336). | Not proven that limit was TV-specific. UI gives no warning when cutting off. |
| #256 | Next ep. appears early and does not autoplay | Unspecified | PARTIAL | Appearing early: credits chain (MKV chapter, IntroDB, sibling ep, learned offset): dee639f7 (src/credfonte.c, src/player.c). 8s advancing countdown: src/posplay.c:326-334. | Autoplay failing to trigger not reproduced. Platform and log missing. |
| #257 | FYI: included in Tizen Community Packages | Samsung | NOT A BUG / OUT OF SCOPE | Third-party notice (Apps2Samsung). | Note: they fetch 4 .tpk and .wgt from release 'without modification'. Keep asset filenames. |
| #258 | Arabic subtitle does not work | Samsung .tpk (S90F) | UNADDRESSED | Same root cause as #253 (no Arabic font outside LG): src/text.c:1147-1152. | No log or details beyond photos. |
| #260 | Optional RTL layout | All | UNADDRESSED | grep RTL/rtl in src/*.c: only dlopen. No RTL layout. | Major feature; will not land without owner decision. |
| #261 | Arabic subtitles do not work | LG webOS (impex) | PARTIAL | Same fix as #239 (bidi/shaping). | No details (file, codepage). Could be Windows-1256 (#247). |

## Method Notes

- Every line RESOLVED IN 2.0 was verified reading the code at HEAD, not just commit message.
- Most of these fixes have host (Mac) tests, not TV tests. Where owner lacks physical proof, it is noted in last column.
- Owner comments on issues ("fixed in 1.x") were treated as clues, not proof.
- Reporter data (links, logs) was not copied. No secrets read.
