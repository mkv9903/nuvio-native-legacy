# Handoff — Release 1.8.0 (Glass UI)

For whoever builds 1.8.0 from `feat/glass-ilha`. Written on 10/03/2026.

## Branch Status

| Ref | Where | What it is |
|---|---|---|
| `feat/glass-ilha` | see `git log -1` (worktree `/private/tmp/nv-glass`) | Complete Glass UI. ~212 commits that master doesn't have. Nothing pushed. |
| `origin/master` | `1345620b` | Published v1.7.3 and v1.7.4. 56 commits that `feat/glass-ilha` doesn't have. |
| Common base | — | `feat/glass-ilha` branched from `release/1.7.2` (merge `c7278fc7`). |

1.8.0 needs both lines: first bring master into Glass UI, then finalize the release.

## Still Running (not in `feat/glass-ilha`)

Merge before release if ready, or leave out and mention in notes:

- `agent/codex-context` (`/private/tmp/nv-codex-ctx`): documentation only (`docs/glass-ui-contexto.md`).

## TV Guide: Partial (already in `feat/glass-ilha`)

Header (kicker, title, chips, segmented Cards/List) and grid as an island are complete. Missing in Glass UI: left hero, category and addon panels, guide search, help bar with pill keys, and background veil. There is no mockup frame for the main page; it was extrapolated from `aovivo-guia` (player-mock) and DESIGN.md.

## Steps

1. **Clean worktree** from `feat/glass-ilha`, e.g.: `git worktree add -b release/1.8.0 /private/tmp/nv-180 feat/glass-ilha`. Never in main repo directory: it contains uncommitted owner work (`fix/tpk-selo-hdr`).
2. **`git merge origin/master`**. Expected conflicts:
   - `src/detail.c`: master changed details significantly (expanding actions, episode badge, Apple TV, title above logo). Glass UI rewrote the page (`agent/detalhe-glass`). Keep Glass UI layout and port behavior fixes from master: episode runtime from addon/TMDB (`0f7254d5`), translated title fallback (`6d74a0ac`, `2bf250fe`), centered cast and visible filmography (`e1a3f83d`), secondary actions when "More" receives focus (`84a31d60`, `6bac65c0`) — in Glass UI style.
   - `src/home.c`: master has hero trailers after empty rows (`09a70f44`), manifest counters (`64ca4854`), wait for disabled manifest (`ce13d98c`), spotlight without animated Home underneath (`1eee19b6`). Glass UI modified Dynamic fade, context menu card, and queued row clipping. Keep both.
   - `src/ajustes.c` and `ajustes_ux_*.inc`: master added options (Discord #222, Seekr, Where to watch). **Enum `OpcaoId` only grows at the END**, and `valor[]`, `CHAVE[]`, `ESC(...)` and `ajustes_ux_padrao.inc` are positional: merge entries in same order across all four lists. Check with `tests/ajustes_secoes.sh` and `tests/ajustes_padroes.sh`.
   - `src/idioma_*.h`: merge lines. Then reorder `src/idioma_tab.h` by **decoded bytes** (`\xNN` decoded before comparison; table is binary search), run `python3 tools/idiomas.py --sincronizar` and fill in 28 languages. `tests/i18n.sh` must report "all ok".
   - `src/text.h` / `src/text.c`: new `TXT_*` styles only at END of enum, with `ESTILOS[]` in same order.
   - Discord (`1ea1510b`, `f515a5cc`, `78a5f222`): Discord Settings screens should use island material.
   - `src/novidades*.c`: master has 1.7.4 card ("animated highlights card", `872ca223`). In 1.8.0 rule is: **new install only sees 1.8.0 card** (`novidadesfila.c` records older markers). Ensure 1.7.3/1.7.4 card enters older markers list.
3. **Compile and run only what matters**: `tests/i18n.sh`, `tests/ajustes_secoes.sh`, `tests/ajustes_padroes.sh`, `tests/novidades_fila.sh`, `tests/acentos.sh`, and full compilation via a harness (`bash tests/fontepref_shot.sh /tmp/x`). The owner requested not running full test suite.
4. **Version 1.8.0** in both files: `deploy/app/appinfo.json` (`"version"`) and `tools/tizen-config.xml` (`version=`). Commit only this, message `v1.8.0`. `tools/env.sh` aborts if they disagree.
5. **Packages** (full recipe in `docs/HANDOFF-1.7.1.md` and project memory):
   - Always `export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"` before any build in worktree, and `bash tools/env.sh --require-core >/dev/null` (never print output). Without this package lacks login, addons and log.
   - LG: `bash tools/arm.sh --ipk` (normal) and then high-cache. `arm.sh` deletes `./*.ipk` at start: copy normal one first. Verify binary file mode: `tar tvzf data.tar.gz | grep nuvio-proto` must be `-rwxr-xr-x` (1.7.1 shipped 705 and would not launch).
   - Samsung: `tools/release-samsung.sh` (.wgt, .wgt Tizen 4, .tpk 6+, .tpk 4/5). New worktree needs `build/ass-wasm` (symlink to `~/.cache/nuvio-ass-wasm`).
   - Android: `bash tools/release-android.sh` (key `~/.nuvio-android/release.jks`; never generate another).
   - **Credentials excluded from packages**: `ar p <ipk> data.tar.gz | tar tz | grep 'art/<file>$'` for trakt.txt, addons.txt, tmdb.txt, mdblist.txt, sessao.txt and collections.json, and `unzip -l <wgt> | grep -E '\.txt|collections'`. All 0.
6. **Notes** in English, `## Added` / `## Fixed`, short bullet points (app card displays 3 lines per bullet and truncates `## Notes`). 1.8.0 highlights below.
7. **Publish only with owner approval**: `git push`, tag `v1.8.0`, `gh release create` with all attachments (2 .ipk, 4 .tpk, .wgt, .apk, libnuvio .so, `repo.json` + `webosbrew.manifest.json` generated by `tools/hb-repo.sh`, SHA256SUMS for LG+Samsung+Android). Download back and check sha256. Release without `repo.json` breaks Homebrew for everyone.

## What Enters 1.8.0 (for notes)

- Glass UI: every panel becomes an island in clock island material, in glass or solid (Appearance). Glass opacity and Frosted glass (advanced).
- Settings v2: thumbnails with real artwork in each category, animated toggles, horizontal growth bars; large categories menu, focused category expands in two rows, list slides over menu; Background (Art / Blurred art / Frost); real previews and charts (memory, diagnostics, speed test); User guide with 87 features.
- Interface size (100/120/130/150%); Spotlight min. 130%, Schedule and Library top min. 120%, sidebar fixed at 90%.
- Sidebar: Modern as floating rail expanding into panel; Standard as bordered panel, centered.
- 18 accent colors (6 new), dark text on light colors, From art / Gradient / Immersive, Texture (cropped title logo).
- New Sources sheet: Best for this TV first, groups by resolution and HDR/SDR, filters MP4 only / Cached / Dubbed, Title logo, Colored badges, custom badge packs (from account or URL), resolution recognized in live channels.
- Player: components originate from island (audio, subtitles, episodes, error, loading), all-black opening with island only, Seekr as island, translated "Live".
- Clock island: confirmations with Undo, flying cover on save, prompt for save location on first use, Update card in 12 states.
- Movie and show details, Schedule, Search (with People), Library (side arrows, context menu on posters and lists with summary), enlarged Social sidebar, app on-screen registration with 6-letter code (server already published), text-measured keyboard.
- Home: channels remain in guide; queued row hidden; Dynamic shows portion of row above.

## Unverified on TV

Almost everything above was verified only via capture on Mac. TCL (Android) received local builds up to `04cbd3f3` (1.7.6 local). Nothing was installed on LG or Samsung. Verify at minimum: all-black player opening, Texture and Frosted glass performance on C9, new sidebar, Sources sheet on LG (6 buttons).

## Pitfalls

- Parallel agents use worktrees in `/private/tmp/nv-*`. Add files **by name**, never `git add -A`.
- Mac disk has only 2–4 GB free: delete binaries and temporary captures.
- Recommendations/registration server already supports 6-letter code (`7cbb25e6`); no new deploy needed for 1.8.0.
