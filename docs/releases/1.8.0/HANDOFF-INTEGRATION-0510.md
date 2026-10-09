# Handoff — Nuvio 2.0 Integration (Formerly 1.8), 10/05/2026 ~16:40 (Updated)

Reader: the agent taking over integration coordination. Read entirely before merging anything.
For performance there is a dedicated document: [HANDOFF-PERFORMANCE.md](HANDOFF-PERFORMANCE.md).

## Location

| Item | Value |
|---|---|
| Integration checkout | `/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy` |
| Branch | `codex/integration-180-glass` — **unpushed** (public release = v1.7.4) |
| HEAD | `13848088` (+ this document). **Two sessions merge into this identical checkout** — check `git log -1` before each merge |
| Owner repo | `/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy` (branch `fix/tpk-selo-hdr`) — **do not touch** |
| 2.0 Mockups | `/Volumes/ExternalSSD/nv-analise/mockups-20/` |
| Issues/logs audit | `/Volumes/ExternalSSD/nv-auditoria/` (`ISSUES-MATRIZ.md`, `LOGS-ANALISE.md`) |

## How to Work (Agreed with Owner)

- The coordinator **does not program directly in integration checkout**: subagents work in dedicated worktree on SSD
  (`git -C <integração> worktree add -b <branch> /Volumes/ExternalSSD/nv-<nome> HEAD`), coordinator merges `--no-ff`.
- Sonnet by default; Opus solely for tough root causes, state/concurrency or major redesigns.
- `export TMPDIR=/Volumes/ExternalSSD/tmp` (internal disk nearly full).
- After each merge: focused tests on changed items ("fewer tests, faster execution") + clean full Mac build:
  `cc src/*.c -o /Volumes/ExternalSSD/<x>-bin -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations`
- Settings merges conflict at end of enum `AJ_*` (`ajustes.c`, `ajustes_ux_padrao.inc`, `ajustes_ux_dados.inc`): concatenate blocks in merge order and update last-option assertion in `tests/ajustes_ux_dados.c`. New options require icons in `ajustes_ux_visual.inc` and i18n across all `src/idioma_*.h`.
- **Install on TVs strictly when owner requests.** Do not send remote keys while he navigates. Evaluate visuals via actual TV captures (Montserrat), not Mac fixtures (Inter).
- Backend (D1 `nuvio-recomendacoes`, worker) **solely with explicit owner OK**, step by step.
- Never post issue replies without approval. Never log/store tokens or credentials.
- English commits ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Build and Deploy (When Requested by Owner)

1. `git -C <integração> worktree add --detach /Volumes/ExternalSSD/nuvio-180-apk-<sha> HEAD`
2. Bump `1.7.4 → 1.8.0` in `deploy/app/appinfo.json` and `tools/tizen-config.xml`, commit.
3. `export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"`; `bash tools/env.sh --require-core`.
4. Android: `bash tools/release-android.sh` → `build/release-1.8.0/Nuvio-1.8.0-android.apk`; verify `/v1/registro` in `libmain.so`;
   `adb -s 192.168.1.128:5555 install -r <apk>` (outside sandbox); pull installed APK and match sha256.
5. LG C9: `NUVIO_SSH_OPTS="-o ProxyCommand=none -o ConnectTimeout=10" bash tools/arm.sh --high-cache` (always high-cache; matches md5 and relaunches).
   Logs: `sshpass -p alpine ssh -o StrictHostKeyChecking=no -o ProxyCommand=none root@192.168.1.32`, log in `/tmp/nuvio.log`.

**On TVs now (10/05 ~16:30):** Android **and** LG = `13848088` (build `/Volumes/ExternalSSD/nuvio-180-apk-13848088`, apk sha256 `b8546d20…`, LG md5 `cf68c2f5…`).
If adb states `No route to host` with TV on: `adb kill-server && adb connect 192.168.1.128:5555`.

## Completed Today (10/05)

- `29aad584` performance handoff; `443b1f5d` neutral splash #0E0F12 without logo.
- **D1: migration 009 (poll) applied** (owner approved). `--file` produces `Authentication error [code: 10000]`; use `--command "<sql without comments>"`.
- **Worker: secret `SIMKL_CLIENT_ID` registered** (owner provided value; never logged). Active worker (from `394f9b7a`) already had Simkl route; only difference from HEAD is `enquete.js` (deploy pending).
- Merges: Frost / Blurred artwork via Immersive lighting pipeline (`83a8e7ad`); Simkl+Letterboxd on Friends tab (`e0686df6`); Settings scenes by category (`0c8a0f9d`) and block/submenu (`5ca65e27`); OLED dimmer + player UI brightness (`c2cd5a15`); bold subtitles using real Montserrat Bold face rather than SDL_ttf synthetic bold (`13848088`).
- Published mockups: What's New 2.0 https://claude.ai/artifact/4BTSfCUDQ1CJ7MrxWm49S7 (approved, implementing) and Explore https://claude.ai/artifact/TMzk2mta36E4PhFPbbH6GR (awaiting selection).
- Secondary stacked subtitle + custom style **already existed** (`ee4bc8a3`, `AJ_LEG2_*`: "Beside primary"); owner had not noticed — search "secondary subtitle" in Settings.

## TV Verification Items (Owner testing pending)

- LG `/tmp/nuvio.log`: `[cor] fundo frost|borrada conferido` or `CONFERIDO ERRADO`; dumps `/tmp/nuvio-fundo-<tipo>.bmp` and `-assado.bmp` (written 1x per run).
- `[leg] negrito: face Bold real de Montserrat, 36 px` when playing add-on SRT subtitles (owner style: Montserrat 90, bold, no outline).
- Dimmer (5 min, 60% → 92%), wake key does not activate action, OSD brightness 80% in HDR, shaking Magic Remote wakes display.
- Simkl: link in Friends tab with owner account (untested against live API).
- C9 draws UI in 1920x1080 and panel is 3840x2160 (TV upscales); rendering in 4K costs ~4× GPU — untouched, owner assumed already 4K.

## In-Flight Agents (Coordinator does NOT receive automatic notifications — check worktrees)

| Task | Worktree / Branch | State ~16:40 | Upon Completion |
|---|---|---|---|
| What's New 2.0 per mockup (opens 1x on 1st launch of 2.0, including fresh install; Social shows "Trakt, Simkl, and Letterboxd"; polls "Depends on server") | `/Volumes/ExternalSSD/nv-wn20`, `feat/whatsnew-20` (Opus) | 2 commits (`fb588471`, `2e6afd35`) + dirty `src/novidades20.c` | Merge, compile, guide tests + `ajustes_ux_dados`; present captures to owner |

If agent disappears without completion (token limit), resume with fresh agent pointing to `git -C <wt> log/diff`.

## Awaiting Owner

- Choose Explore variation: https://claude.ai/artifact/TMzk2mta36E4PhFPbbH6GR (recommendation: B + grid C as entry + button in Detail).
- Deploy poll worker (`npx wrangler@4 deploy --config servidor/recomendacoes/wrangler.toml`) + seed initial poll ([N3-POLL.md](N3-ENQUETE.md)).
- Profile/stats page (redesign; mockup v2 deemed unpolished, deferred).
- Refreshed/animated movie page for 2.0 (mentioned only, no mockup).
- "Refreshed Classic" logo (asset creation).
- Post issue replies (drafts in `ISSUES-MATRIZ.md`).
- D1 size (4.2 GB).

## Known Technical Debt

- Pre-existing tests failing prior to `88d00591`: `detail_layout` and `home_hero_layout` (fixed in that commit; if returning, check stub `desc_pedir_titulo_semente` and 11th argument `friendsH`).
- Untracked file in checkout: `docs/releases/1.8.0/HANDOFF-NEXT-AGENT.md` (from another session; do not delete, do not commit without verification).
- Performance queue: see `HANDOFF-PERFORMANCE.md` (detail carousel on C9, Samsung long tasks, Android onTrimMemory, downscaling large textures, etc.).
