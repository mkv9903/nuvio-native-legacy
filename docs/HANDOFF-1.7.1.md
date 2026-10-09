# Handoff 1.7.1 (10/02/2026)

Base: `origin/master` = published v1.7.0 (b07bb926 + owner docs). Working branch: **`release/1.7.1`** (worktree `/private/tmp/nv-171`; `/private/tmp` was erased 3x — only committed work survives). Version in files is still 1.7.0 (bump = commit `v1.7.1` in `deploy/app/appinfo.json` + `tools/tizen-config.xml`).

## Already merged into `release/1.7.1` (Mac tests pass; NOTHING proven on TV except where noted)

| Issue / origin | Cause (measured) | Fix | Commits |
|---|---|---|---|
| #212 watched badge / eye / "mark watched does nothing" | Badge and eye only checked `progress >= 90`; marking reset progress; Trakt history (`limit=100`) only had episodes | `cat_watched()` single rule; `/sync/watched/movies` (only redownloads if `/sync/last_activities` changed); shows via `/progress/watched`; new setting `SETTING_WATCHED_BADGE` (enabled) | 5f5174ad 24a14ed0 964bb741 |
| #213 (1) Trakt CW with already-watched shows | Entire show marked = same `watched_at` on all episodes; picked middle episode as "latest" | Tie-break picks highest S/E; each candidate checked against `/shows/<id>/progress/watched` (3 in parallel) | 28064cae |
| #213 (2) "Ends at 1" | Translated phrase (ru 35 bytes) in a 32-byte buffer | `clock_end()` with 128 bytes; test covers 30 languages | ff1c8bbc |
| #213 (3)(4) hero/CW in English for 1-2 s | Trakt CW came with art/text from Cinemeta (en); translation was in-memory only | art from metadata addon; localized text saved in `loc-texto.txt` per account/profile | 5658d0b0 |
| #213 (5) gray splash screen | default webOS splash | `splashBackground` in appinfo + `src/abertura.c` (brand, spring, 350 ms..1.5 s) + Android windowBackground | 880a4f76 8557e459 |
| Logs 1.7.0: crash on "catalog remounted X to X+1" | title id stored in 23 bytes (live channel, AIOM collections, anime) -> re-inserts title every frame, memory rises to SIGSEGV (tpk 1.7.0; tizen 1.6.5 27->107 MB) | id with 64 bytes | 2ed8cc02 |
| Seekr (owner request) | seekr.tv = timeline thumbnails only (not addon) | Settings > Integrations > Seekr: enable (off), masked personal key (seekr.txt, per TV), test, filmstrip, sync -60..+60 s. Tested on Mac with real key (Matrix 817 frames, sheet 3200x1800 ~22 MB decoded, released after seek). Key NEVER in file | b3512ba9 ee198248 fc32733c fb66d609 097b41ed |
| i18n | 13 new keys (Seekr) + watched badge across 28 languages | | bf736a5a |
| Merge #212 x #213 in `src/trakt.c` | bracket `}` lost in merge | fixed | 7a5896fa |

Test on TCL (Android 14, release APK of HEAD, 10/02 11:45): account and Trakt ok, `[trakt] watched movies: 100`, `next checked on Trakt: 20 show(s), 4 without next`, `[startup] finished in 934 ms`, no crash.

## TCL test findings for whoever continues
1. `[startup] without brand (.../files/res/art/marcas/nuvio_wordmark.png)`: the PNG IS in the APK; Android only re-extracts assets when version changes (stayed 1.7.0). Disappears with bump to 1.7.1, but extraction should check APK `lastUpdateTime`/hash, not just version.
2. `watched movies: 100` exactly — verify if `/sync/watched/movies` is paginating/limiting (should be the full map).

## Logs 1.7.0 (triage, informational only)
Player errors come from sources (Pluto 401, IPTV 429, unsupported codec, decoder reclaimed when moving to background). Long tasks on .wgt = WASM startup (lower rate than 1.6.5). "Catalog quota" and "ASS fallback" are informational. New features (island, Spotlight, system keyboard, Library/Saved) without errors in logs. Crashes without cause: 1 .wgt (8x, TV closing?) and 3 TCLs (5x, log truncated at end).

## Pending responses (post only when release is live)
- #212, #213: drafts in agent reports (see commit messages).
- #211 LG UK6540 won't open: asked for firmware and `tail /tmp/nuvio.log`; awaiting reply.

## In branches, OUTSIDE 1.7.1 (owner decision)
- `agent/plugins` (QuickJS + htmlq, 29 real scrapers tested on Mac; needs TV and per-scraper choice)
- `agent/p2p` (nuvio-engine/libtorrent, `-DNV_P2P_MOTOR`; needs C9/tpk/Android)
- `agent/icones` (supporter icons, gate `supporter_active()`=0; RISK: activity-alias changes launcher entry for everyone on Android)
- `agent/settingsux` (proposal `docs/settings-ux.md`, phase 1 awaiting OK) and `agent/settingsvisual` (skins A/B)
- `agent/sidebarorg` (stopped by request)
- `agent/guiaunicode` (another session: `\u` in Live TV genres)
- .tpk keyboard/STT = canary `NUVIO_TPK_TEXTO=1` (in master since 1.7.0, disabled in build)

## Recipe (see release recipe memory)
`export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"`; `/private/tmp/NuvioWeb-0.3.38-beta` with `@webos-tools/cli` (ares-package) and local.properties symlink; libass WASM: `source ~/emsdk/emsdk_env.sh; sh tools/build-ass-wasm.sh` (copy in `~/.cache/nuvio-ass-wasm`). adb to TCL may need to run outside sandbox (`No route to host` with port open). Packages for 1.7.0 in `~/.cache/nuvio-rel-170`.

## Addendum: "Type via mobile phone" (agent/celular, already merged)
Every field using src/teclado.c shows a QR code to paste text from mobile phone on the same network (src/celular.c: HTTP server only while keyboard is open, single-use token, 5 min, 4 KB, no CORS, log without text). Proven on Mac and TCL. MISSING: LG (jail/firewall allows high port? log "[mobile] server up"/"page opened"), .tpk, iPhone/Safari; .wgt does not have it (browser cannot listen). MISSING: translating new phrases (i18n.sh fails because of them). Spotlight and Main Search do not use teclado.c (outside).

## Addendum 2 (10/02 ~13:00 BRT)
Merged after first handoff (Mac ok; TV partial):
- **agent/celular2**: mobile button (src/celbotao.c) next to EVERY text field (Spotlight, Search, teclado.c: guide, Saved, nickname, codes, keys). OK opens card with QR on top; received text searches directly in Spotlight/Search. Server only starts with card open. NuvioActivity.kt re-extracts assets on every install (resolves "without brand" in startup). Proven on TCL (end-to-end curl). Screen tests (celbotao_shot, teclado_shot, ilha2_shot, spotlight_shot) FAILED in last run only due to FULL DISK on Mac — re-run.
- **agent/ilhasaida**: exiting player midway with clock enabled returns to HOME and art shrinks down to island mini poster; new setting AJ_SAIDA_PLAYER ("On leaving player": home / title page). Path inside app.c without automated test; not verified on TV.
- Side effect of tests on TCL: "Blue Lights" ended up on owner's watchlist/Saved due to lost OK press — verify.

**ACCOUNT SERVER OUTAGE (measured 10/02 ~13:00):** api.nuvio.tv /rest/v1 returns 504 (auth/v1/health 200) — on Nuvio side. This causes #214 (QR HTTP 400) and #215 (no addons + login HTTP 504) and TCL without addons. Short response draft ready (not posted; owner needs to approve): "The Nuvio account server (api.nuvio.tv) is currently returning errors (504)... don't sign out... next update keeps your last add-ons".
- In progress: **agent/offline** (base release/1.7.1): disk cache of last good addons/collections/library/watched/order list per account+profile, warning "server unavailable", login with clear message and without wiping session on 5xx; investigate 400 in #214. Merge when finished.

Mac disk reached 136 MB free (tests failed because of this); freed 8.5 GB deleting only build/ from agent worktrees and old packages. Remains ~99% full.
