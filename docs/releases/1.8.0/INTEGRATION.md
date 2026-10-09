# 1.8.0 Integration — Glass UI

Review record from 10/03/2026. Continuation and gates: [closure plan](PLAN.md) and [Git inventory](INVENTORY.md). The local integration brings together Glass UI and fixes published in 1.7.4. This document does not confirm a release is ready or installed on TV.

## Source and Scope

| Line | Commit used in integration |
|---|---|
| `feat/glass-ilha` | `16b475b16ac4869207d1ea52edbcd0035f651f7b` |
| `origin/master` | `1345620bd2cee96a830b659f67d17765c0c84c43` |

Integration worktree: `/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy`. The original checkout, with uncommitted owner work, was preserved. 1.7.4 is already public and must not be republished.

The [handoff](../../HANDOFF-1.8.0.md) guides the union of both lines. Consulted visual rules are in `/private/tmp/nv-codex-ctx/docs/glass-ui-contexto.md`; its observations from older builds do not represent the current state of integration. More recent decisions from the owner supersede older measurements in that document.

## Preserved Behaviors and Adjustments

- **Apple TV:** preserve reading veil, enlarged text when scrolling down, blocking title changes while details are expanded, secondary actions exiting the first button upon receiving focus, and add action on click. Following visual correction by the owner, the logo comes first, buttons sit directly below it, and metadata information comes after buttons. The localized title only appears as metadata when artwork has a confirmed foreign language; without a logo, the title occupies the artwork space. The block sits lower in the card and expanded details, without large synopsis or summarized cast line below buttons. The cast section remains available on the page. Long movies constrain offset by measured height; seasons/episodes maintain independent positions. Keep runtime, replay indicator on watched items, and remaining time on incomplete items; centered cast, back navigation, and direct opening of details via filmography.
- **Settings v2:** dedicated scale of 80%, 90%, or 100%, with default at 90%. Measurement, drawing, and navigation use the same factor; the option does not alter global app zoom.
- **Modern Menu:** vertically centered and 20% larger. This is the current decision, replacing the old rule of menu below clock and fixed 90% scale. Apple TV keeps its custom geometry and navigation; Default keeps edge alignment.
- **Glass Material:** Apple TV menu, Sources sheet, Guide category and addon panels, and send-to-friend modal go through shared material helpers, respecting opacity and Frost. Check focus, veil, contrast, and solid mode in final captures.
- **Home and Player:** preserve loading activity in the island, 1-second confirmation for loaded Home, next episode blur, and resume fixes after seek and overlap with Skip Credits.
- **Social:** update data without reusing an earlier account generation; distinguish loading, private, error, and empty. Live presence uses green dot. Reaction and completion are independent states; activities across different episodes are not merged.

## Performance Diagnostics

The 128 MB or 300 MB referenced here represent the image/texture cache budget, not total process RAM. Diagnostics can test a larger budget and restore the previous one when retesting does not justify keeping it; therefore, a 300 MB candidate does not guarantee that will be the final value.

The fix ensures both presentations consult the actually active profile after decision and restoration. The before/after comparison only presents an increase as applied when it was retained. `tests/diagnostico.sh` passed with real cache: improvement, regression, noise, cancellation, manual selection, subsequent change, and persistence. On the host, the transition is 96 -> 160 MB; isolated policy also covers Android 128 -> 300 MB. This transition has not yet been measured on a physical TV.

## Validation Evidence

The integration owner confirmed 13 targeted checks on the host:

| Area | Verification |
|---|---|
| Translations and options | `i18n.sh`, `ajustes_secoes.sh`, `ajustes_padroes.sh`, `acentos.sh` |
| Menu and materials | `menu_geometry.sh`, `glass_surfaces_contract.sh`, `ctxmenu_contract.sh` |
| Island and updates | `ilhafila.sh`, `novidades_fila.sh` |
| Details and services | `detail_layout.sh`, `ondever_folha.sh` |
| Social | `recomenda_social.sh`, `socialvis.sh` |

Four backend cases also passed using real SQLite in `servidor/recomendacoes/teste-social-semantica.mjs`. The full suite was not run.

Temporary host captures are in `/tmp/nuvio-180-review/`. Settings 80/90%, centered Modern menu, Apple TV menu with Streaming, Sources, Guide, and send-to-friend in glass/Frost and solid were checked, along with contextual menu outside selected card. Material and translation verifications were repeated after adjustments. Apple TV captures prior to owner visual confirmation did not validate requested ordering. Local fix uses logo -> direct buttons -> info, removes large synopsis and summarized cast line, preserving cast section and recommendation. New review captures use prefix `apple-no-cast-*`; these are host fixtures without physical validation yet. Compiling and capturing on Mac does not prove behavior on LG, Samsung, or Android TV.

The owner's subsequent request restores compact synopsis to screens and requires consistency between hero/detail in Modern and Apple TV, in addition to fixing bottom line of veil during downward scroll. The implementation captured above still hides the entire Apple TV synopsis; it does not satisfy this new closure. Tasks U01–U04 in [plan](PLAN.md) include drawing/measurement, continuous transition, and performance per platform, preserving logo first, buttons below, and absence of inline cast.

**Subsequent execution direction:** owner requested restoring legacy details layout. Visual reference adopted is `v1.7.4`; new Glass page design was suspended and its patch saved. Restoration must maintain functional fixes and subsequent integrations, without re-adding inline cast. Captures cited above document previous state, not approving final visuals.

## Boundaries Before Release

- Canonical Social identity and complete unification of channels remain pending. Current implementation combines Nuvio and Trakt; full Simkl and Letterboxd adapters do not exist yet. Do not announce all channels or statistics as unified.
- Build seen on TV belongs to another development line. Its 1.7.7/10707 tag was used solely for test installation per owner clarification; does not define a published release. Identify code by commit and APK content. No installation, packaging, publishing, or deploy of this candidate was performed in this round, including local backend changes.
- Merge base was closed at `b98bd2f5`, on branch `codex/integration-180-glass`; monthly Agenda from other conversation entered in `e6885991`, preserving work in progress from other conversation. Version file changes to 1.8.0, packages, and on-device validation must be recorded separately when completed. The [notes](NOTES.md) remain a draft.

### Details Restoration — Local Validation

Per owner request, details layout reverted to `v1.7.4` visual reference, preserving expandable actions, episode duration/progress, and current navigation handlers. Inline cast below synopsis remains removed; cast section remains.

Independent review found and fixed three regressions: localized title for foreign logo must not depend on decode, season summary sits above pills, and Frost is independent of artwork availability. `tests/detail_layout.sh` passed after fixes.

Frozen validation source: `/Volumes/ExternalSSD/nuvio-180-execution-51fxfkxd`, file hashes in `manifest.json`. Integrated capture build passed; Apple fixture passed for actions, expansion, episodes, and long title. The same build executed standard captures, including episode blur. Library capture also passed and partially scrolled row was inspected.

Captures inspected in `captures/`: `restored-174-carousel-expanded-film-logo.png`, `restored-174-carousel-expanded-film-long.png`, `restored-174-apple-episodes.png`, `restored-standard-10-filme.png`, and `library-restored-salvos-cartaz-rolado.png`. These are local fixtures with test data: they do not constitute physical validation or owner visual approval. No new installation in this round. The frozen source still predates additional fixes from review #233 and ongoing memory diagnostics.

### Diagnostics and Synchronization — Final Review This Round

#233 received two additional fixes in review: invalid JSON does not become authoritative deletion; moving a row also skips sources disabled by account or absorbed by a collection. Parser/sync passed with sanitizers, and real Home passed again in snapshot (`captures/233-final-home.log`).

Diagnostics now separates candidate, effectively tested profile, active, and persisted. Failure to write does not announce approval; manual change does not promote candidate when switching back to Automatic; recovery of approved profile respects manual choice. Timeout and write contest decision atomically, preserving result screen. Restore that fails on disk reports effect only for this session and permits retry.

Final independent review approved; 19 focused scenarios, ASan and TSan passed. Root repeated `tests/diagnostico.sh` with real cache on updated frozen source: PASS (`captures/diagnostic-final.log`). No ceiling was raised. 300 MB remains a candidate, subject to measured gain and rollback; this does not prove the cause of the physical 128 MB report. Testing on Android/LG/Samsung and final packaging remain pending.
