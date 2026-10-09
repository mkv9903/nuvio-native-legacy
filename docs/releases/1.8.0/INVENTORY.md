# Git Inventory for 1.8 Closure

10/03/2026. Local inspection of 38 development checkouts; build directories on SSD and packaging copies were excluded from the table. The state is a snapshot, not a guarantee that nothing changed subsequently.

- 22 HEADs were already ancestors of the integration; 16 were not ancestors; 11 checkouts had local changes.
- Comparison used integration `e6885991`; `git cherry` was applied to key candidates to distinguish equivalent patches from merely different commits.
- Uncommitted files remain pending comparison, even in an ancestor branch.
- Main checkout preserved with 90 modified/untracked files. Do not merge/commit/reset on it.

## Destination by Origin

| Origin | HEAD | Relation | Different / Equivalent Patches | WIP | Destination |
|---|---|---|---|---:|---|
| `fix/tpk-selo-hdr` | `2131cb93` | divergent | 1 / 1 | 90 | Audit Library delta 2131cb93 and 90 WIP by family; do not replace new modules or wipe checkout. |
| `agente/ajustesvisual` | `011021c6` | divergent | 2 / 0 | 0 | Previous Settings experiment; preserve as reference, without replacing Settings v2. |
| `agente/guiaunicode` | `6b0f0659` | divergent | 1 / 0 | 0 | Code and Unicode fixture already present in integration; `tests/jscadeia.sh` passed on 10/03. Do not re-apply entire commit. |
| `agente/i195f` | `2aa724f4` | ancestor | 0 / — | 4 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/icones` | `e6010365` | divergent | 5 / 0 | 0 | Selectively integrate the 5 icon/alias commits and test Android upon closure; verify #226/#234. |
| `teste/ondever-171` | `2bc9659b` | ancestor | 0 / — | 80 | Committed code already incorporated; review WIP files before closing this origin. |
| `teste/ondever-android` | `77313de8` | ancestor | 0 / — | 109 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/p2p` | `526eabcf` | divergent | uncompared | 0 | Previous line; use p2p2 as candidate and avoid duplication. |
| `agente/p2p2` | `b1d20394` | divergent | 13 / 0 | 0 | Port engine/APIs/builds after cancellation/budget/cleanup review; real test per platform. |
| `agente/plugins` | `1a3dafcd` | divergent | uncompared | 0 | Previous line; use plugins2 as candidate and avoid duplication. |
| `agente/plugins2` | `e908c412` | divergent | 21 / 0 | 0 | Port modules/HTTP per call/UI after global limits/generations/cancellation; 21 exclusive patches in comparison made. |
| `agente/tr166` | `233e3222` | ancestor | 0 / — | 1 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/codex-ajustes-arte` | `b3d870bc` | divergent | 3 / 0 | 0 | Compare 3 art/animation deltas with Settings v2 and already existing visual decision; do not import another layout. |
| `agente/codex-contexto` | `563c658d` | divergent | uncompared | 0 | Reference document; incorporate effective decisions, older build observations are not current state. |
| `feat/discord-presenca` | `6b6bfe14` | divergent | 1 / 0 | 0 | Semantically compare with already functional Discord 1.7.4; different patch does not mean absent feature. |
| `agente/escala` | `81169b99` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/glass-ajustes` | `a57e0dea` | ancestor | 0 / — | 1 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/ilha-tudo` | `44260bda` | ancestor | 0 / — | 1 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/fix-menu` | `94e9a6eb` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/fix-modais` | `83cd1e42` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/fix-social` | `2e2b28a5` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/fix-telas` | `5a60f592` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/fontes-cab` | `4afe0390` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `feat/glass-ilha` | `16b475b1` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/glass-ui` | `430c2dc7` | ancestor | 0 / — | 1 | Committed code already incorporated; review WIP files before closing this origin. |
| `agente/player2` | `a7648756` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/player3` | `6542d317` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `agente/selos-pacote` | `a8b60da8` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `codex/cache-home-perfil` | `9b17d38d` | ancestor | 0 / — | 2 | Committed code already incorporated; review WIP files before closing this origin. |
| `codex/integration-180-glass` | `e6885991` | ancestor | 0 / — | 58 | Active integration line; commits by ownership after tests, WIP is not final package. |
| `codex/pr-230-integration` | `66af5817` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `codex/fix-addons-158` | `5ca5c825` | divergent | 4 / 0 | 0 | 4 commits of Social documentation only; incorporate useful contracts, do not confuse branch name with new fix. |
| `feat/android` | `77313de8` | ancestor | 0 / — | 0 | Already incorporated; no new merge needed per consulted HEAD. |
| `feat/home-dinamica-padrao-hero` | `48a4966e` | ancestor | 0 / — | 11 | Committed code already incorporated; review WIP files before closing this origin. |
| `fa4e7bd18996eaa27fd2abef10a98d40651e8f5d` | `fa4e7bd1` | divergent | uncompared | 0 | Historical mockup/analysis; inspect per file, do not import 36 commits as finished feature. |
| `webos3` | `373088d1` | divergent | 5 / 1 | 0 | Compatibility/art/diagnostics: review useful deltas; port requires dedicated validation, not full merge. |
| `feat/tizen4-coop` | `99c3bed9` | divergent | 5 / 0 | 0 | Older cooperative experiment; keep separate, without replacing current TPK40 path. |
| `feat/vidaa` | `a81639d2` | divergent | 7 / 0 | 0 | Separate experimental port; preserve patches, own matrix, #135 does not imply guaranteed release. |

## WIP of Main Checkout by Responsibility

| Family | Comparison Required |
|---|---|
| Addons/parser/JS/network | Bounds/body/URL guards, partial sources, and buffers versus current base and new plugins/stream-fit; preserve boundary tests |
| Player/ASS/Tizen | MKV/subtitles/tracks/HDR/ROI/TLS, libs, and manifests versus PR230 and already published fixes |
| Account/progress/cache | Account/profile, syncprog/vistoep/progress, and cache/threads versus #233 and new CW |
| Home/Settings/Library/icons | Unicode, end of row, scrolling, artwork, and options without reintroducing old layout |
| Tools/versions/documentation | Separate generated experiment/build from runtime; test version is not release version |

Do not stage all files out of convenience. Each family concludes with origin, effective delta, test, and destination; generated artifacts are investigated before any cleanup.

## Priority Candidates and Risk

- Plugins2 `e908c412`: 21 exclusive patches in this comparison. Generic HTTP must be ported in additive hunks; keep current TLS/headers/final URL/TPK40/Discord. Review DOM/JS allocation, queue, and code before accepting.
- P2P2 `b1d20394`: 13 exclusive patches. Disk ceiling, cancellation, writers/cleanup, and toolchains still require fix/proof; do not enable solely based on stub success or SKIP test.
- Icons `e6010365`: 5 exclusive patches. Aliases/splash/menu need navigation test and data preservation; do not create a new Android signature.
- Unicode Guide `6b0f0659`: behavior already integrated; `jscadeia` test passed. Library `2131cb93`: viewport/fade hunk ported to Glass Library, preserving wave and contextual menu; syntax passed, integrated capture pending.
- Social Documentation `5ca5c825`: 4 commits of owner decisions; incorporate into canonical implementation, do not count as ready Social.

## Consolidation Gates

- [ ] Reconcile the 11 WIP development checkouts, file by file; keep work of other owners.
- [ ] Record each chosen, equivalent, or superseded patch in the commit closing the family.
- [ ] Finish current fixes and run a serial core build of the integration.
- [ ] Freeze sources and use a separate manifest for each test/release package.

Execution plan: [PLAN.md](PLAN.md). No branch was deleted/archived by this inventory.
