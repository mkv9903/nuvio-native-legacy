# Handoff 2.0: Everything Remaining Until Release (10/05/2026)

To incoming agent: this is the SINGLE document of what remains to publish 2.0. Complete the tasks in the order below. Read the entire file before editing. Nothing here authorizes publishing: tag, release, push, `avisos.json`, and issue replies require explicit owner approval.

## Work Order

1. **Task 1** — Embedded Arabic font on Samsung + Windows-1256 subtitles (7 Arabic issues).
2. **Task 2** — Code fixes fitting in 2.0 (#254, #255, #252) and pending checks.
3. **Task 3** — Remaining TV proofs (Android, LG, Samsung).
4. **Task 4** — Release 2.0.0 (packages, notes, publication only with authorization).
5. **Post-Release** — Concise issue replies and log requests.

## Where Everything Resides

- Integration checkout: `/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy`, branch `codex/integration-180-glass`. HEAD at handoff time: `cdc000aa` (or commit above if prior session committed more). Everything **local, without push**.
- `master` = last published release (1.7.4). 2.0 originates from integration.
- Triage of 44 open issues against 2.0: [ISSUES-2.0.md](ISSUES-2.0.md). One correction: #226/#234 (launcher icon on Android) **are already** in integration (`src/iconeapp.c` + `activity-alias` in `AndroidManifest.xml`); report stated they were only in `agente/icones`, which is incorrect.
- Prior handoffs (context, already incorporated here): [HANDOFF-NEXT-AGENT.md](HANDOFF-NEXT-AGENT.md), [HANDOFF-BACKGROUNDS-SCREENSAVER.md](HANDOFF-FUNDOS-DESCANSO.md).

### Merged into Integration Today (10/05)

| Commit | What |
|---|---|
| `6a13452c` | merge feat/perfil-fundos-descanso (Movies/Light/Projector backgrounds + screensaver) |
| merge selos | feat/selos-xperience (embedded badge packs) |
| `9ca29519` | `.tpk` 4/5: curl error buffers without `_Thread_local` (`tools/tpk.sh` rejected) |
| `09124343` | static screensaver (no Ken Burns), guide 2.0 with "Screensaver" chapter, single profile background migration to Movies |
| `247688b4` | merge PR #251 (.tpk subtitle counts window from 1st frame) |
| `709feaad` | screensaver clock relocates each minute with 1.5s fade |
| `d300f927` | source sheet: spacing between title logo and badges; "Where to Watch" icon 56 px |
| `359b8000` | detail: logo height settles smoothly; artwork returns to top 2x faster |
| `cdc000aa` | search/Spotlight: clears requested term when targets rebuilt (previously found people only after changing add-ons) |

### NOT Entering 2.0

- **PR #259 (DTS on webOS)**: tested on C9 in branch `test/dts-259` (`/Volumes/ExternalSSD/nuvio-dts-259`). C9 plays DTS natively (PR path does not even activate). Both times PR activated (Top Gun, AIOStreams 2160p), it failed: `Selected DTS track or video absent` (likely TrueHD, not DTS) and `Container open: Input/output error` (its range reader failed to open link). Left for later; owner decides whether to comment on PR. "Nuvio Legacy DTS Debug" app (`space.nuvio.native.legacy.dtsdebug`) is installed on C9 alongside normal app.
- Host `.tpk` canaries (`agente/i195*`, `agente/i203`, `canary/*`, `canario/*`), `webos3`, `feat/vidaa`, `feat/tizen4-coop`, `perf/trim-fileira` (duplicate of `tex_pressao_memoria`).

---

## Task 1 — Embedded Arabic Font (Samsung) and Windows-1256 Subtitles

### Rationale

7 Arabic issues (#239 #245 #247 #250 #253 #258 #261). Letter shaping and RTL order are merged (`src/bidi.c`, 6bb186ac merge), but **Samsung lacks Arabic font**: letters render as squares (#253, #258). On LG and Android it works because system provides fonts.

### What Code Already Does

- `src/bidi.c` performs Arabic joining by converting to **Presentation Forms-B** (U+FE70–FEFF) and parts of Forms-A (U+FB50–FDFF, e.g. U+FB56..FB7D for پ چ). Zero HarfBuzz. That is: embedded font must include **these presentation form blocks**, not merely base block U+0600–06FF.
- `src/text.c` ~1120–1160: fallback table by script. `ESC_ARABE` lists only system fonts (LG `/usr/share/fonts/DroidNaskh-Regular.ttf`, `LG_Display_Urdu.ttf`, macOS, Android `/system/fonts/Noto*Arabic*`). CJK scripts terminate with `cjkEmbarcada` (`<base>fonts/DroidSansFallback-Subset.ttf`), and **this is pattern to follow**: append `arabeEmbarcada` at end of `ESC_ARABE`.
- `NUVIO_SEM_RESERVA_DE_SISTEMA=1` on Mac simulates Samsung WASM (strictly what is in `deploy/app/fonts`). Use for TV-less proof.
- Full font pack packages across all targets: `tools/tizen.sh` uses `--preload-file deploy/app/fonts@/app/fonts`, `tools/tpk.sh` uses `cp -R deploy/app/fonts`, webOS and Android package `deploy/app/fonts`. New files added automatically.

### Steps

1. Font: **Noto Naskh Arabic Regular** (or Noto Sans Arabic UI), OFL license. Download official TTF (github.com/notofonts/arabic, release). Save license alongside, matching `DroidSansFallback-LICENSE.txt` and `*-OFL.txt`.
2. Subset (pyftsubset from fonttools, following `tools/fonte-cjk.py` pattern):
   `U+0600-06FF, U+0750-077F, U+FB50-FDFF, U+FE70-FEFF, U+0020-007F, U+00A0, U+060C, U+061B, U+061F, U+066B-066C, U+200C-200F`.
   Write `tools/fonte-arabe.py` regenerating `deploy/app/fonts/NotoNaskhArabic-Subset.ttf`. Size target: tens to few hundred KB. Samsung WASM preloads entire package in RAM: keep under ~300 KB.
3. `src/text.c`: `char arabeEmbarcada[600]; snprintf(..., "%sfonts/NotoNaskhArabic-Subset.ttf", base);` and append to `ESC_ARABE` (before `NULL`), matching `cjkEmbarcada`.
4. `tools/idiomas.py` verifies glyph coverage of embedded fonts (item 10 of docstring). No `ar` table (#250 is new feature, outside 2.0), but run `python3 tools/idiomas.py --cobertura` to verify regressions.
5. Mac proof: Arabic subtitle and Arabic title with `NUVIO_SEM_RESERVA_DE_SISTEMA=1` (see `tests/` with "arabe"/"bidi" — `grep -l bidi tests/*.c`). Capture before/after: squares → joined RTL letters.
6. Commit in Portuguese or English per repo convention, with `Co-Authored-By`.

### Windows-1256 Subtitles (#247, #261) — Implement Concurrently

`src/legenda.c` ~495–560 detects only UTF-8/16, 1251, and 1252. Legacy Arabic subtitles arrive in **cp1256** and render as garbage despite bidi and fonts. Detect (high frequency of bytes 0xC1–0xED with low Latin ASCII) and convert via 128-entry table (0x80–0xFF → Unicode). Test with `.srt` in cp1256 (generate via `iconv -t CP1256`), matching existing subtitle tests.

### Outside 2.0 (Owner Decision)

- #250 (Arabic UI) and #260 (RTL layout): large features.

---

## Task 2 — Code Fixes Fitting in 2.0 and Verification

Source: [ISSUES-2.0.md](ISSUES-2.0.md). Tasks doable without reporter:

- **#254 (Samsung .tpk 6) Home does not refresh on add-on install/removal, "ghost" catalogs.** Zero commits address. Reproduce on Mac: install and remove add-on with catalog and collections, verify whether rows appear/disappear without restarting. Related to today's fix `cdc000aa` (search did not rebuild on add-on change): examine other points depending on `desc_alvos_busca_zerar` / add-on list retaining stale state.
- **#255 (Samsung .tpk 6) Home displays only 5 collections.** Unproven whether cutoff is TV-specific; UI lacks warning when capped. At minimum: clear log of cutoff and explicit ceiling notice if present.
- **#252 Default audio language for anime.** Untagged language track ignored in auto-selection (`src/video.c` ~884). Minimal fix: untagged track must not lose when only track of its type; "anime-only" setting is feature, excluded.
- **#249** resolved for series; comment in `src/posplay.c:411` mentions 52% and is stale — update comment.
- **#241** trailer zoom on `.tpk` remains Off by default: confirm with owner (help warns black screen on some Samsung displays).
- **#226/#234 launcher icon on Android**: code exists (swaps `activity-alias` upon app exit). On TCL zero aliases swapped yet. Ask owner to toggle icon in Settings and exit to Home, then inspect `adb logcat | grep app-icon` + `dumpsys package space.nuvio.nativelegacy | grep -A12 enabledComponents`.
- **"Minha TV" add-on** (`*.baby-beamup.club`) belonging to owner times out on manifest (12–20s). Private server; verify Home does not wait for it.

---

## Task 3 — Remaining TV Proofs

| Device | What | How |
|---|---|---|
| Android TCL `192.168.1.128:5555` | Detail: Ratings → top and "logo grows from nowhere" (`359b8000`) | Owner navigates; `adb logcat` lines `FPS=`, `[quadro]`, `[gpu-modos] ... tela=detalhe`. Before: 37–41 FPS and `fill=4.7–4.9x` on return. |
| Android TCL | Spotlight finds titles after add-on changes (`cdc000aa`) | Add/remove add-on and search identical term. |
| LG C9 `192.168.1.32` | Build of today's changes (`tools/arm.sh --high-cache`) | `ssh -o ProxyCommand=none root@192.168.1.32` (password alpine); log in `/tmp/space.nuvio.native.legacy.log`; check `/proc/<pid>/exe`. |
| LG C9 | Screensaver: static showcase, clock shifts position with fade, zero time on showcase | Settings › Appearance › Screensaver 30s. |
| Samsung | Zero items from today proven. Priority: #233/#254/#255 and Arabic | Only owner/reporters have Samsung hardware. |

The `.tpk` 6+ generated earlier (`LG WEB/nuvio-tpk-build/build/tpk/Nuvio-1.7.4-NuvioTpk60.tpk`) is **stale** (precedes #251, static screensaver, search, detail). Rebuild for release.

---

## Task 4 — Release 2.0

**Version: 2.0.0** (owner decided 1.8 becomes 2.0). Require owner confirmation prior to publishing anything: tag, GitHub release, push, `avisos.json`, and issue replies are public.

### High-Cost Rules Learned

- **NUVIO_PROPERTIES** always exported. Outside `LG WEB/` `tools/env.sh` misses `local.properties` and package outputs **without keys** (zero login, sync, Discord). Occurred today on APK. Proof: in app log, `[nuvem] https://api.nuvio.tv`, never `[nuvem] SEM CONFIGURACAO`.
  `export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"`
- **ares-package**: NuvioWeb tree copy missing. Installed in `/Volumes/ExternalSSD/ares-cli`; `export NUVIO_ARES_PACKAGE=/Volumes/ExternalSSD/ares-cli/node_modules/.bin/ares-package`.
- **Version in two files**: `deploy/app/appinfo.json` and `tools/tizen-config.xml` must match (`env.sh` rejects divergence). `tizen-tpk/*/tizen-manifest.xml` stamped by `tools/tpk.sh`.
- **`tools/arm.sh` without `--build` installs on C9** of owner. On C9 always `--high-cache`.
- **Credentials**: verify every package before uploading (`ar p <ipk> data.tar.gz | tar tz | grep -E 'art/(trakt|addons|tmdb|mdblist|sessao)\.txt|collections\.json'`, `unzip -l <wgt|tpk|apk> | grep -E '\.txt|collections'`). Must return zero.
- **Binary permissions in .ipk**: 1.7.1 shipped 705 and failed to open. Verify 755.

### Packages

1. LG: `bash tools/arm.sh --ipk --build` and `bash tools/arm.sh --high-cache --ipk --build`.
2. Samsung `.wgt`: `bash tools/tizen.sh` (requires `~/emsdk`; libass WASM in `~/.cache/nuvio-ass-wasm`).
3. Samsung `.tpk` 6+ and 4/5: skill `samsung-tpk` / `samsung-release`; `bash tools/tpk.sh` in clean worktree (e.g. `../nuvio-tpk-build`, existing, sibling to NuvioWeb, locates keys automatically). Docker running (`nuvio-tpk-sdk` image).
4. Android: `bash tools/release-android.sh` (key in `~/.nuvio-android/release.env`; rejects without `NUVIO_PROPERTIES`). TCL preview APK (`192.168.1.128:5555`) built with `tools/android.sh` + temporary 1.8.0 stamp; for release, use release script with genuine 2.0.0 version.
5. English release notes in `docs/releases/2.0.0/NOTES.md` (`## Fixed` / `## Added`, single bullet per item, human-written tone; see What's New memory). Source: app guide (`src/novidades20.c`, chapters) + summary in `ISSUES-2.0.md`.
6. Publishing (only upon owner authorization): fast-forward merge into `master`, `git tag v2.0.0`, `gh release create v2.0.0` with all attachments and `SHA256SUMS`, download back and verify sha256 checksums, `repo.json` / Homebrew (`tools/hb-repo.sh`), `avisos.json`. Reply to resolved issues concisely.

### Hardware Unproven State

- Android: today's build is on TCL. Owner felt detail page was sluggish navigating from Ratings to top; fix (`359b8000`) installed without his confirmation yet.
- LG: today's changes build being installed on C9 as handoff was written.
- Samsung (.wgt and .tpk): zero changes from today proven on Samsung. Major risks: #233/#254/#255 (catalogs and collections on .tpk) and Arabic font from Task 1.
- #223 (Android 11 black screen on login) and #211 (LG webOS 4 exits on startup) depend on reporters.

---

## Post-Release

- Reply concisely (owner guidance: "nobody reads a bible") on resolved issues: #223 #231 #233 #235 #237 #238 #239 #243 #244 #245 #249 and on "resolved earlier" (#188 #197 #202 #209 #227) asking to close.
- Request 2.0 logs: #211 (LG webOS 4 exits on open), #228 (trailer on Q80A), #232 (unblurred thumbnail), #240 (Torbox delay), #246 (anime HEVC seek), #256 (next episode autoplay), #158 (C4 Live TV), #171 (P2P/Torrentio on .tpk).
- PR #259 (DTS): owner decides whether to comment. Findings at top of file. Remove "Nuvio Legacy DTS Debug" app from C9 upon instructions.
- PR #251 merged: thank KeijoMika and close PR referencing commit `247688b4` (merge was local in integration).

### Account Keys (Measured Today, Names Only)

`sync_pull_provider_credentials` per profile: profile 1 has only `tmdb` (**v4** token, app ignores and uses package key); profile 2 has only `mdblist`. Debrids, IntroDB, and AnimeSkip empty. Zero trakt/imdb/omdb/fanart in account. Nothing to do for release; document if queried.
