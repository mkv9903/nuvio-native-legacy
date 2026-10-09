# Handoff — Nuvio 2.0 Integration, 10/05/2026 11:46 (-03)

Continues [HANDOFF-INTEGRATION-0510.md](HANDOFF-INTEGRATION-0510.md): workflow rules, build/deploy recipes, and
paths remain valid and are not repeated here. Read that first, then this document.
Performance: [HANDOFF-PERFORMANCE.md](HANDOFF-PERFORMANCE.md).

## State

| Item | Value |
|---|---|
| Integration | `codex/integration-180-glass` at `4eec55a7`, unpushed |
| Android TV (TCL, 192.168.1.128) | APK from `0c8a0f9d` (worktree `/Volumes/ExternalSSD/nuvio-180-apk-0c8a0f9d`), sha256 verified |
| LG C9 (192.168.1.32) | `dcef5c14` high-cache (worktree `/Volumes/ExternalSSD/nuvio-180-apk-dcef5c14`) — **without** submenu images |

Another session writes directly to integration checkout: `8364262f` and `4eec55a7` (test stubs only) are not from this
coordinator. Check `git log` before each merge.

## Merged in this Session

| Merge | Branch | Description | Evidence |
|---|---|---|---|
| `83a8e7ad` | `fix/frost-ambiente-lg` (`f1f5034d`) | Frost and Blurred Art via ambient lighting path; single dump `/tmp/nuvio-fundo-<type>[-assado].bmp` | **C9: Frost correct on title page** (`[cor] fundo frost conferido`, screen = expected 94,40,35; 59–60 fps static). Captures in `docs/reviews/frost-c9-0510/` of owner repo |
| `dcef5c14` | `feat/social-simkl-letterboxd` (`e0686df6`) | Simkl and Letterboxd rows in Friends tab | Mac only. Unseen on hardware |
| `0c8a0f9d` | `feat/ajustes-imagens-submenus` (`3c2386ee`) | Category/submenu image in Settings + 0.22s crossfade | Mac only; owner saw on Android and requested more (track below) |

Open items from these merges:
- `3c2386ee` introduced early return in `gfx_rect` (`src/gfx.c`) when `gfx_opacidade_grupo <= 0`. Shared code; unreviewed whether
  any caller depended on drawing at 0 opacity.
- Frost dump runs across all platforms with writable `/tmp` (one fullscreen `glReadPixels` per type, per run).
  Frost agent was instructed to place behind diagnostic flag.
- Root cause of dark bake on C9: unproven. Simply bypassed that path.
- Blurred Art never viewed on C9 (owner TV set to Frost, `fundoLocal 2`).

## In-Flight Agents (Coordinator does NOT receive automatic notifications — check worktrees)

All branch from integration, work solely on Mac, unpushed, committing in dedicated worktree. Upon completion: `git merge --no-ff`
into integration, focused tests, full compile. If agent disappears without commit, resume with fresh agent from `git -C <wt> diff`.

| Track | Worktree / Branch | Model | State 11:46 | Owner Request (Summarized) |
|---|---|---|---|---|
| Frost: 2 Android defects | `nv-frostamb`, `fix/frost-ambiente-lg` | Opus | 1 modified file, no new commit | (1) Home **Modern** + Frost: background sits **on top of hero**. (2) Blurred Art correct in Settings, **dark on movie detail**. Reproduce via Mac captures before/after in `/Volumes/ExternalSSD/tmp/frost-android/`. Unproven hypotheses: out-of-order fullscreen opaque pass; 28% veil applied twice or bake run before artwork arrived. On Android dump does not write (`/tmp` does not exist for app) |
| Keyboard 2.0 | `nv-teclado`, `feat/teclado-20` | Opus | 37 modified files, uncommitted | Navigation: field and modes (TV keyboard / Speak / Type on your phone) are on LEFT but reached only via UP (`fileira == -1` in `src/teclado.c`) → Left enters, Right returns. Caps lock (single tap = next char, double = lock) and symbols layer, without typing outside field alphabet. Phone QR inside modal (`celb_abrir` draws in corner currently), without breaking Spotlight and Search. Captures in `/Volumes/ExternalSSD/tmp/teclado-shots/`. Return with restricted alphabet field list for owner decision |
| Settings: items & pills | `nv-ajitens`, `fix/ajustes-itens-toggles` | Opus | 4 modified files, uncommitted | (1) "Reorder and enable rows": **missing line for items outside Home** — locate commit and restore. (2) Advanced toggle in index shows **eye only, no text**. (3) Top 3 pills **slightly larger and colored**. (4) **Every option understandable**: if scene does not illustrate focused option, custom scene responding to value; inventory in `docs/settings-visual-map.md`. Captures in `/Volumes/ExternalSSD/tmp/ajitens-shots/`. Observed on TV: Background "Blurred art" and "Frost" thumbnails nearly identical and dark — pass to this track |
| Explore 2.0 | `nv-explorar`, `feat/explorar-20` | Opus | 1 modified file, uncommitted | Owner: "proceed with explore per your recommendation" = **B (Rabbit Hole) primary + grid C as entry from sidebar + "Explore from here" button in Detail**. A not built. Contract = mockup `/Volumes/ExternalSSD/nv-analise/mockups-20/06-explorar.html` (published: https://claude.ai/artifact/31siKmoEB3qk46SGjsuXMH). Priority: `mapa_vizinhos` + B + Detail button → C with local catalog → `/discover` → Save trail. Remote lacks "I" key from mockup: agent maps and coordinator **informs owner which was mapped**. Captures in `/Volumes/ExternalSSD/tmp/explorar-shots/` |
| Player: opening source + spinner | `nv-abrindo`, `ui/abrindo-minimal` | Sonnet | commit `63df0d20` (frameless spinner) completed; 6 modified files in progress | (1) Opening source **minimalist**: source badges only; **Down** reveals details (full name, add-on, latency, download speed…); never URL. (2) Upon load completion, **shrinks and dismisses like clock island**. (3) Player spinner **without opaque gray background**, dots only. If Down already assigned during loading, agent reports and coordinator **queries owner**. Captures in `/Volumes/ExternalSSD/tmp/abrindo-shots/` |

Likely merge conflicts: `nv-ajitens` and `nv-teclado` modify i18n (`src/idioma_*.h`, `idioma_tab.h` byte-sorted — sort via script, not by hand); `nv-explorar` and `nv-abrindo` also append keys. Merge one by one and run `python3 tools/idiomas.py` after each merge.

## Awaiting Owner

- Review captures from each track before deploying to TV (owner triggers deploy; do not install autonomously).
- Blurred Art on C9: change setting to test, solely with approval.
- Approve What's New 2.0 (https://claude.ai/artifact/4BTSfCUDQ1CJ7MrxWm49S7).
- Deploy poll worker + seed initial poll.
- Secret `SIMKL_CLIENT_ID` on worker (without secret, Simkl row responds 501 and disappears). Agent report notes migration 008 + F08 deploy also pending — **unverified in D1**.
- Untouched from prior handoff: profile/statistics, refreshed movie page, "Refreshed Classic" logo, issue replies, D1 size.

## Bench Lessons Learned Today

- LG: OK on Home hero during startup launches fullscreen trailer; screenshot returns **black with logo** because video plane does not enter BMP. Not broken screen.
- LG: busybox `sleep` rejects fractions; never run `find /` on TV (holds ssh >60s).
- LG capture: `echo 1 > /tmp/nuvio-shot-req; chown 5152:5000 /tmp/nuvio-shot-req` → `/tmp/nuvio-shot.bmp`; keys in `/tmp/nuvio-key` with same `chown`, 2s between keys.
- Android: `adb exec-out screencap -p` is passive and usable while owner navigates; `run-as` non-functional (non-debuggable package), so `ajustes.txt` cannot be read directly.
- `tools/arm.sh` output prints compile line with `local.properties` keys. Redirect to file on SSD and read only tail (`==> conferindo`), never paste in chat.
- `wrangler d1 execute --file` fails (code 10000); use `--command`.
