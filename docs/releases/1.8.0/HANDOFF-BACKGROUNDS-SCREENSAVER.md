# Handoff: Profile Selection Backgrounds + Screensaver (2.0)

Worktree: `/Volumes/ExternalSSD/nuvio-180-fundos-descanso`, branch `feat/perfil-fundos-descanso`, base `6e4bdfb3` (integration `codex/integration-180-glass`). WIP commit at end of session. Not YET merged into integration.

Owner-approved mockup: https://claude.ai/artifact/5XkkA6NoVho9DDw1E5zKy5

## Owner Decisions (10/05)
1. Default: **Movies** on capable TVs, **Light** where GPU scales down to minimal effects (implemented in `perfilsel.c:modoFundo`).
2. Screensaver: default **2 min**, options Off / 30 s / 1 / 2 / 5 / 10 min.
3. Showcase: dual source options (Catalog / My List and Continue Watching).
4. Screensaver on profile screen as well: yes (dimmer runs across all screens).

## Completed (Compiles on Mac; dimmer tests pass; inspected captures)
- `gfx.c/h`: `gfx_girar(rad, cx, cy)` / `gfx_sem_girar()` — `uGiro` uniform in vertex shader, initialized to (1,0,0,0) in every program, disabled on internal passes (ESC_REAL) and fullscreen clear shortcut.
- `text.c/h`: style `TXT_DESC_HORA` (230, weight `PESO_FINO`), font `deploy/app/fonts/Montserrat-ExtraLight-Relogio.ttf` (digits and `:` subset, 8 KB). Loaded via same path as mono (`caminhoMono[2]`).
- `esmaecer.c/h`: 6 choices (`esmaecer_ms`, default `ESM_PADRAO`=3 → 2 min); style (`ESM_ESTILO_VITRINE/RELOGIO/ESCURECER`); with showcase/clock: 5s of 60% veil followed by 100% black + `descanso_desenhar`. `esmaecer_segura_protetor_tv()`. Test `tests/esmaecer.sh` updated and passing.
- `descanso.c/h` (new): showcase (Ken Burns, logo or title, meta+genres, 1st plot sentence, cascade, rotates every 20s, preloads next, "OK to open" strictly with Home focused) and clock (thin numerals, minute accent bar, date, next Schedule premiere, anti burn-in path ±140×80 px, brightness ~62%).
- `main.c`: OK with active screensaver → `descanso_pedir_abrir()`; `esmaecer_estilo(player_aberto() ? ESCURECER : ajuste)`; `descanso_quadro(...)` per frame.
- `app.c`: `descanso_pedido_abrir()` → `abrirPorIndice(k)` if `app_na_home()`.
- `video.c` (webOS): LG screensaver response also suppresses when Nuvio screensaver active (`segurar = 2`); `protegerScreensaver()` invoked at end of `iniciar()`. LIMITATION: registers only after video module boots (first trailer/movie). Verify on C9 that LG screensaver does not overlay.
- `ajustes.c` + `.inc`: `AJ_DESCANSO_ESTILO`, `AJ_DESCANSO_FONTE` appended to enum (`descansoEstiloLocal`, `descansoFonteLocal` keys); `V_ESMAECER` with 6 values, labeled "Screensaver"; `V_PS_FUNDO` = Movies/Stripes/Profile Art/Light/Projector; inactive states; icons; search keywords; new "Screensaver" section in Interface (`ajustes_ux_tela.inc`). Settings preview (`ajcHomePerfil`) fixed: 1 and 2 were swapped (Stripes showed blurred artwork); Light/Projector received basic previews.
- `psparede.c/h` (new): per-profile wall in `perfilparede.txt` (12 posters: progress → list → Home start), saved in `perfilsel_iniciar` for ACTIVE profile; PIN unpersisted; purged on logout (`sync.c`).
- `psestilos.c/h` (new): rendering of Movies (wall rotated −0.16 rad, 9 alternating columns, 0.9s cross-fade on profile switch, fallback = catalog mural), Light (light follows focus, neighbor, horizon at y=896, grain 640×360), and Projector (beam = GFX_SOMBRA + two rotated blades, dust, film grain, scratches, 0.07 grain).
- `perfilsel.c`: `modoFundo()`, `montarCena()`, calls in init/update/draw.
- Capture tests: `tests/descanso_shot.sh [prefix]` (new) and `NUVIO_PERFILSEL_FUNDO=0|3|4 NUVIO_PERFILSEL_SHOT_PREFIX=... bash tests/perfilsel.sh --capturas`.

## Remaining Items (In Order)
1. **Translations** (28 languages + English). Missing keys in `src/idioma_tab.h`: "Light", "Projector", "Screensaver", "Screensaver style", "Showcase titles", "Showcase", "Clock", "Dim only", "My list and Continue", "%s · S%d E%d · %s", "%s, %d %s", "Sunday"…"Saturday" (7), "Next premiere", "In your catalog", and the 4 new help texts (`AJ_PS_FUNDO`, `AJ_ESMAECER`, `AJ_DESCANSO_ESTILO`, `AJ_DESCANSO_FONTE` in `ajustes.c`). Insert byte-sorted, `python3 tools/idiomas.py --sincronizar`, populate empty slots, `python3 tools/idiomas.py` must pass. Run `tools/varredura-i18n.py`.
2. `tools/arm.sh` / ARM build (webOS GCC is stricter; webOS `video.c` does not compile on macOS). Check if new font packages into `deploy/app/fonts/` across all targets.
3. Run only changed tests: `tests/esmaecer.sh`, `tests/perfilsel.sh`, `tests/psfundo.sh`, `tests/ajustes_padroes.sh` (screensaver default changed from 5 to 2 min), settings options count/ordering test if present.
4. Small visual polish: in showcase, logo can scale to 820×220 (`DESC_LOGO_W/H`); gaps between source/logo/metadata appear wide with transparent-padded logos.
5. On TV (C9, `arm.sh --alto-cache`; capture with `/tmp/nuvio-key` and .bmp): Movies/Light/Projector on profile selection (fps), screensaver 30s with showcase and clock, OK opening title, LG screensaver suppressed.
6. Merge into integration (other session had open conflicts in `ajustes.c`, `ajustes_ux_dados.inc`, `ajustes_ux_padrao.inc`).

## Cautions
- Zero dark central patches on profile screen (owner requirement from 09/21).
- `ajustes_ux_padrao.inc`: default values are positional; appended 2 at end (style 0, source 0) and changed dimmer default to 3.
