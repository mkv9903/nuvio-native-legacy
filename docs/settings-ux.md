# Settings: UX Research and Proposal

Research baseline: commit `399c1953`, on branch `release/1.6.6` at the time. The branch name does not identify the app version: this commit succeeds the version 1.7.0 commit. Proposal revision: **10/02/2026**. Status: **first native deliverable implemented in isolated worktree; unverified with real users or physical remotes**. Historical research below remains identified by its original baseline; current status appears in the following section.

Labels and wireframes below are provisional; final on-screen copy and translations will follow. The inventory describes the baseline referenced, not active production metrics. Local reference `release/1.7.1` was inspected at `be9e413e`; the implementation subsequently incorporated `eba014d9` from 1.7.1. The integrated baseline is merge `2ab3ccc8`. Do not treat divergent master checkouts as a continuation of this baseline.

The owner's request: "rethink how people navigate and tweak settings, so they understand what every single option does and can tailor it to their preference". The 1.6.x visual redesign was rejected ("the problem is still the same, just wearing a different skin"). This document explains why the underlying problem persisted and what genuine structural improvements are required.

## Native Implementation as of 10/02/2026

The catalog now contains **183 options across 11 categories**, with **61 advanced options**. `AJ_SAIDA_PLAYER` was introduced into Playback as basic following the 182-option inventory. The GPU option remains conditional on TPK/Android; the compiled Mac/LG list excludes it. IDs, keys, and persisted value indices are strictly preserved.

- Navigation organized into fixed blocks; advanced visibility remembered per category during session.
- OK opens selection sheet; arrows cycle candidate without persisting; OK applies once; Back discards. Saved value carries dedicated visual marker, independent of focus.
- Dedicated local search with diacritics/synonyms, navigation path, and unavailable state indicators; opens by ID, reveals advanced options, and Back restores query and results.
- Differences calculated against identical immutable factory defaults initializer, without attributing authorship to the user. List preserves scroll position after resetting a row.
- Individual item reset displaying target value/scope with Cancel focused by default; excludes actions, credentials, readouts, searchable profile flags, and dedicated file keys like row limits. Row reordering remains in existing dedicated flow.
- Explained dependencies; when an editable requirement exists, OK navigates directly to it and Back returns to originating option. No requirement is silently enabled.
- Detected failures in main config file preserve previous value and editor state; concurrent external changes invalidate draft candidate. Feedback confirms local application; does not fabricate remote server confirmation or IDBFS flush completion.

### Polish Aligned with Mockup

The initial native build appeared visually heavier than the design prototype; following owner feedback, composition was revised against the mockup reference. Compact title, wider central column, 80 px rows, text values, crisp hairline focus outlines without blur halos, slate blue surfaces, and compact advanced toggle restore the proposal's intended hierarchy. Scope and default values occupy dedicated rows in the explanatory panel. Remote control shortcut hints sit in the footer.

Art samples utilize the prototype's local image, labeled "Illustrative example"; they do not stream trailers, make network requests, or simulate live Home previews. If artwork is absent from package, panel falls back to existing schematic illustration. Settings surfaces remain opaque for legibility, even with Frosted Glass enabled on Home; selected accent color is respected. No modifications to global shaders.

[Native captures and verification](reviews/settings-ux/README.md).

Sources: `src/ajustes_ux_tela.inc`, `ajustes_ux_dados.inc`, `ajustes_ux_interacao.inc`, `ajustes_ux_desenho.inc`, `ajustes.c`, `spotlight.c/.h` and dispatch in `app.c`.

Host validation: catalog, search/defaults/privacy, D-pad scenarios and persistence, defaults, profiles, safe mode, languages, and Spotlight. Native captures use account-less fixtures. This does not substitute for testing physical remotes, semantic screen reader accessibility, rendering performance, or TV storage persistence.

Phases 2A/2B (live Home preview, timeout, and performance presets) and Phase 3 remain proposed. The interactive prototype below demonstrates aspects of these, but they have not yet transitioned into the native renderer. New copy includes EN/ES with fallbacks; comprehensive multi-language review remains pending. [Platform research and direction](ux-direction.md).

## Interactive Prototype

This proposal includes an [interactive local prototype](prototypes/settings-ux/README.md) covering Trailers, Languages, and Performance: 18 options, search, factory diffs, selection sheets, and reversible experiments. Values and synchronization are simulated; the demo does not alter the native app or account state. Browser tests in the README do not replace user testing on physical TVs.

## Review Outcome

The strategic direction remains sound: organize by user intent, articulate consequences, and enable safe experimentation. The proposal needed to finalize interaction contracts, eliminate unsubstantiated claims, and break deliverables into phased stages.

| Priority | Identified Gap | Proposal Decision |
|---|---|---|
| High | Arrows mutate values while also navigating columns | Navigation never mutates values; OK enters editing; help panel is unfocusable |
| High | Factory differences attributed to user without change log | Label as "Modified from default"; show origin/timestamp only when recorded |
| High | Live preview leverages paths that immediately persist and sync | Decouple draft, staging, and commit; canceling restores previous state |
| High | "This TV only" profile contains sync-eligible keys | Restrict profiles strictly to proven local keys; never auto-switch tastes |
| High | Phase 1 claims table-only changes but adds search and state | Split navigation, search, and diff/reset into discrete deliverables |
| High | Mutation logs treated as proof of discovery and causality | Acknowledge sampling bias and validate user workflows directly |
| Medium | 11 categories and 3 columns treated as proven solution | Prototype and benchmark keystroke cost, readability, and comprehension |

Contexts to validate: users wanting to watch content and fix an annoyance; users customizing visual styling; users troubleshooting stutter or playback errors. These are behavioral hypotheses, not interview-derived personas. Evaluation is qualitative: no fabricated usability scores or unverified percentage speedup claims.

---

## 1. Codebase Inventory

Source: `src/ajustes.c` (enum `OpcaoId`, `OPCOES[]`, `CHAVE[]`, `valor[]`, `TELA[]`, `ajudaOpcao`, `efeitoOpcao`, `desenhaPrevia`, `somenteDesteAparelho`). Measured via automated script against source code.

| Metric | Count |
|---|---|
| Options in `AJ_*` enum | **176** |
| By kind | 127 choices (97 binary, 90 using On/Off), 10 numeric, 33 actions, 6 read-only |
| Settings with value (choice + numeric) | 137: 76 sync-eligible to account, 61 local to this TV |
| Keys with "-" (do not persist) | 41 (actions, readouts, external file credentials) |
| On-screen hierarchy | 9 categories › 13 collapsible groups + 9 fixed labels › 176 rows |
| Navigation depth | Settings › category › group › row › (sheet/keyboard/subscreen). E.g., "Trailer on focused poster" = Settings › Layout › Poster focus › row 3 |
| Rows per category | Layout **76**, Integrations 41, Content 14, Advanced 13, Playback 10, Appearance 9, Account 6, About 4, Trakt & Simkl 3 |
| Largest group | "Home Content": 20 rows mixing rows, rail, spotlight, labels, ratings, and profile background |
| Dedicated help copy | 175 of 176 (only `AJ_SALVOS_DEST` falls back to generic text) |
| Have "what changes in practice" note (`efeitoOpcao`) | 14 |
| Have custom preview graphic (`desenhaPrevia`) | 8 options, 5 distinct drawings — all **schematic** (gray wireframe boxes), not real interface |

Enums, types, categories, sync eligibility, and layout mappings were re-verified in this review. Help copy quality and preview ratings reflect original baseline analysis. The 176 rows include `AJ_GPU_EFEITOS`, visible only on TPK/Android builds; other platforms show 175 rows and 12 in Advanced. Account eligibility does not guarantee active synchronization.

**Help Copy Quality.** Text volume is not the issue; text framing is. Auditing all 175 descriptions:

- ~25 merely restate the label or resort to technical jargon: "Controls poster corner rounding", "How much focused poster edge glows", "Use Reduced for subtle motion", "Picks next episode from furthest marked as watched", plus 8+11 rating toggles with identical text.
- Several fail to define what **Disabled** actually does. E.g., Dolby Vision ("Preference for compatible sources") — does disabling filter out DV streams? Play them in SDR? A user on an SDR display needs that exact clarity.
- At least one is misleading by omission: "Item width" — `ajustes.h` notes that under the Modern layout this preference **does not alter size**; help copy omits this critical detail.
- Lengthy entries (Home layout: 377 chars; Row limit: 315 chars) explain well, but turn into dense walls of text unreadable from 10 feet.

**Scattered Topics** (primary root cause of navigation failure):

| Topic | Fragmented Locations |
|---|---|
| Language | Appearance › Language (interface) · Playback › Languages (audio, subtitle) · Integrations › TMDB › Metadata language · subtitle styling only inside player |
| Trailers | Home Layout (spotlight, audio, delay) · Poster Focus (focused poster) · Title Details (auto, audio, button, quality, aspect, source) — 10 rows across 3 groups |
| Ratings | Home Content › General ratings · Integrations › MDBList (8 sources) · Integrations › Title ratings (11 sources) |
| Artwork | Home Layout (Hero background, Alternate spotlight art) · Custom posters (7) · Addon artwork (4) · Integrations › TMDB › Localized art · fanart.tv |
| Title Meta | Details Page (Prioritize external metadata **and** Always use Cinemeta, which overlap) · TMDB (13 toggles and 1 language) |

**Why Visual Reskins Failed.** The existing tree mirrors the web app (`SECTION_META` in `settingsScreen.js`), organized by **where code lives** (Layout, Integrations, Advanced), rather than user intent. "Layout" alone represents 43% of all rows. Reskinning preserves the same 4-level navigation depth, scattered topics, and wireframe boxes.

Solid foundational code already present:

- `TELA[]` is decoupled from enum since 1.4.4: reorganizing layout touches neither `valor[]`, `CHAVE[]`, nor config files.
- `inativa()` already understands option dependencies (and provides rationale).
- `seguro.c` provides automated crash rollback for 10 high-risk settings.
- `ajustes_abrir_na_cor/fonte/layout/vidro` are functional deep-links into specific rows (used by What's New cards).
- Long-press context menu on posters (`ctxmenu.c`) includes "Row style": a proven contextual setting.
- `perfiltv.c` (`PtvPerfil`) sizes memory, networking, and art buffers; does not implement §4.6 visual tiers. `gpunivel.c` supplies GPU diagnostics.
- Spotlight (`spotlight.c`) searches titles, people, channels, catalogs, and addons — **does not index settings**.

### 1.1 Post-Baseline Additions

Local comparison on 10/02/2026, anchored in commits:

| Metric | `399c1953` (baseline) | `be9e413e` (consulted 1.7.1) |
|---|---:|---:|
| Enum options | 176 | 182 |
| Choices / numeric / actions / readouts | 127 / 10 / 33 / 6 | 130 / 11 / 35 / 6 |
| Values: sync-eligible / local | 76 / 61 | 76 / 65 |
| Collapsible groups | 13 | 14 |

Added to proposal map, preserving existing enum and persistence:

| New ID | Proposed Location | Treatment |
|---|---|---|
| `AJ_SELO_VISTO` | Home screen › Rows | Basic; local choice |
| `AJ_SEEKR_LIGADO`, `AJ_SEEKR_FITA` | Playback › Timeline thumbnails | Basic; local choice |
| `AJ_SEEKR_CHAVE`, `AJ_SEEKR_TESTAR` | Same thumbnail block | Basic; setup/test service; masked key, excluded from diffs |
| `AJ_SEEKR_AJUSTE` | Same thumbnail block | Advanced; local time offset |

Baseline covers 116 basic and 60 advanced; incorporating this delta yields **121 basic and 61 advanced** prior to category grouping and platform filters.

TMDB regional language normalization (`pt-br` → `pt`) is already merged in `v1.7.0` (`b07bb926`). Do not duplicate this work. Spotlight integration should target modern branch code. `seguro.c` and `perfiltv.c` are unchanged; constraints in §§4.3/4.6 remain active.

---

## 2. Real-World Telemetry

### 2.1 Device Diagnostics (D1, Aggregated Only)

Baseline sample: ~9.4 days of logs, **265 reporting devices** (webOS 162, Tizen .wgt 73, .tpk 49, VIDAA 5, Android 3, Mac 1). Includes developer test TVs and opt-in diagnostic users.

**Data Caveat:** Platform subtotals equal **293**, not 265. Overlap is possible but unconfirmed. Exact query dates and deduplication criteria were not captured. Numbers are preserved as historical context and **were not re-queried in this review**. Do not treat totals as definitive global percentages.

**No granular per-setting modification logging exists.** Observed signals:

| Signal | Result |
|---|---|
| `[seguro] testing` (10 high-risk settings) | **105 devices (40%)** changed at least one. Frosted glass **93** · immersive theme 35 · row limit 31 · 4K interface 26 · image quality 24 · hero trailer 22 · items per row 16 · P2P 13 · image memory 11 · poster trailer 4 |
| `[seguro] reverted` (app crashed and rolled back) | 11 devices: glass 5, immersive 2, 4K 2, quality 1, P2P 1 |
| `[fileiras] local choice: limit N` (latest per device, 201) | 130 default 7 · 24 legacy default 16 · **47 (23%) custom choice**, range 3 to 40 (6 at max 40) |
| `[tex] limit of N MB requested in Settings` | 35 devices pinned texture memory (14 at max 512 MB) |
| `[gpu-modos] slow` (90 devices, slow sessions only) | glass on in 54 (60%), dynamic theme in 37 (41%), Dynamic layout 17, Standard 3 |
| `[ajustes] account blob` (latest startup, 200) | 120 received remote account changes; 69 uploaded local changes to account |
| `unrecognized; preserved` | `tmdb_language` with unknown value in 16+ devices (`pt-br` 7, `ro` 4, `el`, `es-419`, `ru`, `ar`, `uk`); `selected_theme=CUSTOM/DARK` 3 |

Analysis:
- Among the **10 safe-mode monitored options**, frosted glass generated the highest volume of edits and rollbacks. This does not prove it is the most modified setting overall or the sole cause of crashes. Correlation between glass and slow sessions lacks control group. TV performance profiles and safe experimentation are hypotheses to test, not established causal proofs.
- ~25% of users adjusted row limits — a key layout preference currently tucked into Layout group 1.
- In 120/200 devices, startup applied account changes. This reflects standard sync; logs do not indicate frustration. Surfacing sync scope clarifies behavior.
- `tmdb_language` "pt-br" from account failing to match `W_TMDB_LING` ("pt") caused TV to ignore web metadata preference (#209). Regional fallback has been resolved in subsequent code (§1.1).

### 2.2 GitHub Issues Audit

Summary from baseline research; 204 issues surveyed by title, ~50 reviewed in depth:

| Pattern | Issues |
|---|---|
| **Requested existing feature / unable to find** | #124 (hero trailer existed), #127 (split upcoming existed), #57 (source selection on playback), #183 (trailer source in Settings), #201 (per-row styling behind long-press OK), #71 (readout confused with setting) |
| **Option failed to behave as expected / invisible effect** | #160 (spotlight catalogs did nothing), #162 (Discover location no effect), #133 & #177 (blur failed to apply), #199 (unreleased items and CW source had no effect), #205 (changing CW source resets row), #202 (white text on white background in Settings) |
| **Lack of trust in persistence** | #85, #129, #149 (settings reverted to default on relaunch) |
| **Fragmented language settings** | #3, #8, #150, #209 — user configures Portuguese in app and TMDB but synopses remain English |
| **Screen layout and density** | #11 ("cluttered"), #75 (requested selectable headers to jump), #144 ("settings are really bad") |
| **Customization requests** | #95, #162, #163, #90, #142, #104, #47, #91, #175 |
| **Trailers (most recurring topic)** | 13 issues: #15, #60, #82, #86, #123, #124, #136, #140, #168, #178, #183, #195, #204 |

Maintainer responses frequently required four-level navigation paths ("Settings › Layout › Poster focus › Trailer on the focused poster"), demonstrating that discoverability is the primary issue.

---

## 3. Platform Guidelines and Architectural Decisions

Primary sources consulted 10/02/2026:

| Source | Documented Principle | Application in Nuvio |
|---|---|---|
| [Android — Design for TV](https://developer.android.com/design/ui/tv/guides/foundations/design-for-tv) | 10-foot legibility, low visual density, clear D-pad feedback | Test from sofa; avoid shrinking typography to force 3 columns |
| [Android — TV navigation](https://developer.android.com/training/tv/get-started/navigation) | Predictable directional flow, obvious focus states, D-pad accessibility | Finalize focus transitions, back navigation, and focus restoration |
| [Apple — Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/) | Focus and activation are distinct; prevent accidental changes | Scrolling through options must never activate or persist values |
| [Kodi — Settings](https://kodi.wiki/view/Settings) | Basic, Standard, Advanced, and Expert tiers govern visibility | Test progressive disclosure without hiding search or reset mechanisms |

Overlaying settings on top of active screens, synonym-based search, and contextual resets are key design tenets for Nuvio. Two-layer navigation governs **category → option**; selection sheets, external connections, and confirmation dialogs may introduce sub-steps. Optimize entire user flow, not just hierarchy depth.

---

## 4. Proposed Architecture

### 4.1 Intent-Based Organization

Eleven categories represent an **initial working model**. Baseline inventory encompasses 176 options: **116 basic and 60 advanced**. Incorporating post-baseline additions yields **121 basic and 61 advanced**.

Grouping ratings or rail toggles simplifies related choices, but introduces sub-sheets and extra clicks. Credentials and media providers are multi-step connection flows, not simple radio buttons; do not artificially merge them solely to reduce line counts. Provide canonical homes for all options, with cross-references for settings with multiple reasonable mental models (e.g., poster trailer in both "Posters" and "Trailers").

```
 Search settings               (opens Spotlight pre-filtered to Settings)
 Modified from default (12)    (accessible even when 0 modified)
 ─────────────────────────────
 1  Home screen                layout · rows · Continue Watching · sidebar
 2  Posters & artwork          focused poster · geometry · artwork sources
 3  Trailers                   playback location · audio · source provider
 4  Title details              visible elements · ratings · metadata
 5  Playback                   source selection · video/audio · during/after playback
 6  Languages & subtitles      interface · audio · subtitles · metadata
 7  Appearance                 theme color · glass · typography · animations · clock
 8  Live TV                    provider setup · streaming troubleshooting
 9  Accounts & services        account & profiles · Trakt & Simkl · API keys · P2P
10  TV performance             performance profile · resolution · image cache · diagnostics
11  About & help               version · updates · log submission
```

Mapping (`AJ_` prefix omitted; *italics* = Advanced):

| Category › Block | Options |
|---|---|
| 1 › Layout | HOME_LAYOUT, LANDSCAPE, HERO, HERO_CHEIO, HERO_CATALOGOS, *HERO_TRANSICAO* |
| 1 › Rows | FIL_ORDEM, FIL_LIMITE, ITENS_FILEIRA, OCULTAR_NLANC, NOTAS_HOME, *ROTULOS, NOME_ADDON, SUFIXO_TIPO* |
| 1 › Continue Watching | CW_LIGADO, CW_OK, CW_ESTILO, CW_THUMB, CW_BLUR_PROX, CW_ORDEM, CW_NAO_EXIBIDOS, *CW_FURTHEST* |
| 1 › Sidebar | RAIL_MODERNA, RAIL, MENU_EXPLORAR/GUIA/AGENDA/PERFIL (one row), *RAIL_BLUR, DESCOBRIR* |
| 1 › Profile Selection | PS_FUNDO |
| 2 › Focused Poster | EXPANDIR, EXPANDIR_ATRASO, BORDA_FOCO, *GRAD_CLASSICO* |
| 2 › Geometry | LARGURA_DP, RAIO_DP, PROF, *PROF_BORDA, PROF_BRILHO, PROF_COBERTURA, PROF_POSTERS/CW/EPS/ELENCO/TRAILERS* |
| 2 › Artwork Sources | HERO_FUNDO, HERO_ARTE_DIF, POSTER_PROV, *ADDON_POSTER/FUNDO/LOGO, COL_ARTE_CONTA, POSTER_INST/TOKEN/EXTRA/CHAVE/MODELO/TESTAR* |
| 3 › Playback Location | HERO_TRAILER, FOCO_TRAILER, DET_TRAILER_AUTO, DET_TRAILER |
| 3 › Audio & Video | HERO_TRAILER_SOM, DET_TRAILER_SOM, TRAILER_FONTE, *HERO_TRAILER_ESPERA, TRAILER_QUAL, TRAILER_ASPECTO* |
| 4 › Display Elements | DET_VEU, DET_DATA_CHEIA, DET_BLUR_NAO_VISTOS |
| 4 › Ratings | NT_* (11, unified "Which ratings" row), MDB_LIGADO, *MDB_TRAKT…MDB_MAL* |
| 4 › Metadata Sources | TMDB_LIGADO, *TMDB_ARTE…TMDB_CW (12), DET_META_EXT, DET_SO_CINEMETA* |
| 5 › Source Selection | FONTE_MANUAL, FONTE_AUTO, FONTE_REPOR, *SELOS_CORES* |
| 5 › Audio & Video | QUALIDADE, DV, ATMOS |
| 5 › During & After | PAUSA_OVERLAY, CW_CONCLUIDO |
| 6 | IDIOMA, AUD_LINGUA, LEG_LINGUA, TMDB_IDIOMA (+ shortcut to player subtitle style) |
| 7 | TEMA, COR_LOGO, VIDRO, FONTE_UI, ANIM, RELOGIO, RELOGIO_POS, *VIDRO_CONTORNO* |
| 8 › Provider | XTREAM_SERVIDOR/USUARIO/SENHA/CONTA/LIMPAR, STALKER_PORTAL/MAC/LIMPAR, EPG_PAIS |
| 8 › Troubleshooting | LIVETV_DIAG, LIVETV_RES, LIVETV_ESPERA, *LIVETV_FORMATO, LIVETV_MODO, LIVETV_PROXY* |
| 9 › Account & Profiles | PERFIL_ATIVO, SYNC, ADDONS, ADDONS_PRINCIPAL, PERFIL_PESQ, PERFIL_EDITAR, SAIR |
| 9 › Trakt & Simkl | TRAKT, SIMKL, SALVOS_DEST, CW_FONTE |
| 9 › API Keys | DEBRID_RD/TB/PM/AD/AD_TESTAR, MDB_CHAVE, FANART_CHAVE (single row opening list) |
| 9 › P2P | *P2P_LIGADO, P2P_URL, P2P_TESTAR* |
| 10 | (device profile, new), RESOLUCAO, QUALIDADE_IMG, GPU_EFEITOS (.tpk/Android), NAV_RAPIDA, DIAGNOSTICO, VELOCIDADE, ESPACO, *TEX_MB* |
| 11 | VERSAO_I, ATUALIZAR, ENVIAR_LOG, ENVIO_AUTO |

Additional Architectural Rules:

- **Read-only rows removed from list:** Version, Profile, Sync status, Xtream Account, and Memory Footprint become category header metadata. Resolves #71.
- **Advanced options:** Collapsed by default, but discoverable via search and "Modified from default". "Show advanced" toggle sits at top of list, accessible without scrolling to bottom. Search temporarily exposes matching advanced rows.
- **Dependencies:** Remain visible when disabled, explaining requirement and providing actionable navigation link to prerequisite setting. Do not hide "Trailer audio" simply because trailers are disabled.
- **Platform incompatibilities:** Excluded from standard navigation, but discoverable via search with explanation of device limitation.
- **Stable ordering:** When active filter excludes focused row, move focus to nearest valid sibling or category header. Focus and selection must not rely solely on color.
- **Consolidate overlapping rows:** "Prioritize external metadata" + "Always use Cinemeta" → single 3-way radio choice (underlying dual config keys preserved).

### 4.2 Interaction and Navigation Model

The 3-column layout is an experimental UI model. Columns 1 and 2 participate in D-pad navigation; column 3 is informative. Value editing, resets, and dependency jumps occur via modal selection sheets triggered by OK.

```text
┌─────────────────────┬────────────────────────────────┬──────────────────────────┐
│ Search              │ Trailers                       │ Focused poster trailer   │
│ Modified from def.  │ Show advanced: No              │                          │
│                     │                                │ When resting on poster,  │
│ Home screen         │ WHERE IT PLAYS                 │ plays trailer if device  │
│ Posters & art       │ In top hero           Enabled  │ requirements permit.     │
│ Trailers            │ > On focused poster   Enabled› │                          │
│ Title page          │ On title page         Enabled  │ Disabled: displays still │
│ Playback            │ Trailer button        Enabled  │                          │
│ Languages           │                                │ Scope: this profile*     │
│ …                   │ AUDIO AND VIDEO                │ Default: Disabled        │
│                     │ Audio in hero        Disabled  │                          │
│                     │ Trailer source           Auto  │ OK: options & details    │
└─────────────────────┴────────────────────────────────┴──────────────────────────┘
```

`*` Scope and sync state are dynamically computed for the active key/value. The selection sheet details exact requirements (e.g., poster trailer requires expansion **or** landscape format; safe mode may suspend effect).

| Context | D-pad | OK | Back |
|---|---|---|---|
| Categories Column | ↑/↓ navigate; → enters list | Enters list, restoring last focused row | Returns to originating screen |
| Options List | ↑/↓ navigate; ← returns to categories; → no-op | Opens selection sheet or designated flow | Returns to categories; returns to origin if entered via search/shortcut |
| Choice Sheet | ↑/↓ cycle choices and actions; ✓ marks saved value | Confirms candidate or executes selected action | Discards draft candidate and returns focus to row |
| Numeric Slider | ←/→ adjust draft candidate; ↑/↓ reach Apply/Cancel | Applies candidate only when Apply button focused | Discards draft candidate |
| Multi-select Sheet | ↑/↓ navigate items and Apply/Cancel | Toggles candidate item; Apply confirms set | Discards all sheet changes |
| Sub-dialog / Keyboard | Component-specific | Confirms component value | Closes dialog layer without popping to Home |

Every editable row, including boolean toggles, requires OK to edit. This adds one keystroke but eliminates accidental changes. System actions like logout or pairing never mimic value selectors. Core functionality must never depend on voice control, color buttons, or long-pressing OK.

Back navigation preserves query, scroll offset, category, and focused option ID. If origin was destroyed, land on nearest valid ancestor. Asynchronous background updates must never steal focus.

**Help Summary:** Highlights current effect, consequence of switching, prerequisite status, scope, and factory default. Full text and actions live in selection sheet. Every visible control must be D-pad accessible. Distinguish focus, selection, error, and disabled states with geometry/contrast, never color alone.

Benchmark 3 columns vs. 2 columns with expanded sheet descriptions. On 720p or long translations, prioritize legibility; never shrink font size to fit columns. Verify visible focus, margins, contrast, and reduced motion. Previews must directly illustrate setting effects.

### 4.3 Staged Previews and Reversible Experiments

Three distinct states: **candidate**, **testing**, and **committed**. Scrolling through values never writes to disk, syncs to account, or records changes. Pressing Back, switching profile, or closing flow discards candidate.

1. **In-place rapid adjustment:** Side panel overlaying live UI surface. Background scene renders temporary values. Inspect side effects: certain preferences trigger row rebuilds, asset loads, or external state mutations outside `valor[]`. "Apply" commits; "Cancel" restores previous values. Reuse existing textures; never clone full framebuffers or freeze video to simulate live preview.
2. **Real-art thumbnails:** Reuse cached resources; fall back to labeled generic sample if absent. Show before/after comparisons where helpful.
3. **Reversible experiments:** For high-impact settings (layout re-architectures, video pipeline switches), display affected parameters and Keep/Revert actions. Initial 15-second timer starts only after usable interface renders. Enforce a hard application timeout: if interface fails to render within limit, auto-revert without waiting for user confirmation. Crash recovery in `seguro.c` handles process exits on next boot. Reverting restores entire prior state batch. Only explicit user confirmation permits publishing preference.

**Implementation note:** `mudarValorDireto()` applies side effects, persists to disk, and triggers sync immediately. It cannot serve as a raw preview API. A dedicated staging layer with pre-change snapshotting is required.

`seguro.c` handles crash protection: confirms after 180 s or clean exit, auto-reverting on reboot following crash. This is distinct from a 15 s interactive trial. Coordinate both systems: track transaction batches; prevent clean exit from auto-confirming unaccepted trials; ensure crash recovery takes priority over trials.

### 4.4 In-App Settings Search

- Spotlight adds dedicated Settings domain and standalone mode accessible via "Search settings". In this mode, query only the local settings index; never trigger media or addon searches.
- Index labels, non-sensitive enum values, descriptions, and localized synonyms (subtitles/CC, dubbed, trailer, slow/stutter). Never index passwords, tokens, URLs, profile names, or typed credentials.
- Rank exact label matches first, followed by synonyms and help text; maintain stable ordering. Display path, current value, and availability status.
- Index advanced options. Selecting an advanced result temporarily exposes the row with clear indication; does not toggle global advanced visibility. Dependent results link to requirements; unsupported options explain device limitations.
- Standardize deep-links via `ajustes_abrir_em(op)`. Back restores search query and results.
- Empty query presents local topic chips. Offline and voice-free by design.

### 4.5 Factory Diffs, Provenance, and Rollback

Phase 1 benchmarks active preferences against **current version factory defaults for the platform**. Present active values and runtime safe-mode/capability suspensions separately; do not conflate "Automatic" with resolved runtime values. Differences do not prove user intent or fault. Avoid labeling as "What you changed" until mutation timestamps are tracked.

- Reference an immutable default configuration struct; `valor[]` is mutable runtime state. App updates may alter default diffs without user intervention.
- Display modified advanced options and explanatory empty states. Exclude credentials, readouts, and actions from diffs and resets. Composite options display both underlying keys.
- Differentiate **scope** (device-local vs. profile-synced) and **sync status** ("Saved on this TV", "Pending sync", "Synced"). Display persistence failures clearly.
- Determine sync behavior dynamically: keys marked `"-"` or excluded from sync blobs behave differently based on network state and values.
- "Restore default" reveals target value and scope prior to confirmation. For sync-eligible settings, explain that changes propagate to other TVs sharing the profile.
- "Use account value" is a distinct operation, enabled only when remote state is known.
- Category-level reset follows individual item reset; must display affected batch, exclude secrets, handle partial failures, and support clean undo.

**Change history is a later phase.** Existing values lack origin timestamps; do not fabricate dates. Future logging will distinguish local user action, account sync, migration, recommendation, and crash recovery. Respect `dePerfil()` boundaries.

### 4.6 Device Performance Profiles

Replace generic Low/Balanced/High tiers with hardware-tailored configurations explaining exact trade-offs. Accepting a 4K video signal does not imply GPU can render a 60 fps 4K UI.

Initial implementation targets proven local keys only: UI resolution, image quality/cache size, row limit, items per row, GPU effects (where supported), and frosted glass. Enforce per-platform bounds and use §4.3 experimental rollback.

**Exclude depth effects, poster trailers, and theme palettes from automated performance presets.** `AJ_PROF` and `AJ_FOCO_TRAILER` sync across devices; swapping dynamic themes can trigger account mutations. Never treat visual taste as local hardware tuning.

`PtvPerfil` manages low-level texture buffers and network concurrency; it does not dictate user preference presets. RAM, GPU tier, crash history, and framerate telemetry guide recommendations only after proving cost profiles per platform.

Manual application shows before/after diffs. Preserve user choices on reboot; never re-apply presets automatically. After manual tweaks, label profile as Custom. "Revert to previous setup" restores snapshot batch while respecting safe mode.

### 4.7 Optional First-Run Setup Wizard

Future phase, reserved for complex onboarding scenarios. Target fresh installs lacking saved preferences; never intercept users attempting immediate media playback. Skipping applies explicit defaults; wizard remains accessible later.

Candidate queries: interface language; preferred audio and subtitle languages; automatic vs. manual stream selection; home screen layout; trailer autoplay. Each response displays affected settings and sync scope.

Selections remain staged until final review; Back preserves draft and exiting discards cleanly. Never silently enforce visual profiles based on RAM.

### 4.8 Contextual Shortcuts

Expose settings directly from relevant surfaces while preserving canonical hierarchy:

- Poster long-press menu: keep "Row style" and add surface settings link. All options exist in main Settings.
- Video player: playback settings, audio/subtitles, and source rules, without closing playback. Distinguish current stream track from global default.
- Title page: trailers, ratings, and backdrop dimming, returning cleanly to title details.
- What's New cards: leverage standardized deep-links.

Shortcuts share identical validation, side effects, persistence, and back navigation.

### 4.9 Privacy-Preserving Telemetry

Instrument **committed changes only**, restricted to a strict allowlist of non-sensitive keys. Format example: `[ajustes] confirmed id=<id> before=<enum> after=<enum> via=<screen|quick|search|wizard|recommendation|account>`. Never log freeform text, search queries, profile names, URLs, MAC addresses, passwords, or tokens.

Differentiate opening, confirming, canceling, and failing. Previews and aborted adjustments do not record confirmations. Honor user's log-sharing preferences; generate no secondary tracking IDs.

---

## 5. Implementation Roadmap and Milestones

Validate the complete user journey on a small slice before overhauling all 183 options simultaneously. Prototype slice: Trailers, Languages, and Performance.

| Phase | Deliverable | Hard Dependencies | Acceptance Criteria |
|---|---|---|---|
| Preparation | Updated map, sample copy, interaction prototype | Commit-level inventory; focus and state contract | Visual review and completion of §7 test workflows |
| 1A — Navigation & Hierarchy | Intent-based tree, advanced toggle, summary card, selection sheets without focus-save | `ajustes.c/.h`, localization, existing shortcuts; confirm storage of advanced visibility | 100% ID coverage; zero accidental mutations; reliable focus restoration |
| 1B — Discovery | Local settings search, synonyms, deep-linking by ID, back navigation | `spotlight.c/.h`, `app.c` dispatch, target text input, localized index | Offline search; reveals advanced options; zero media query pollution |
| 1C — State Inspection & Recovery | Modified from default and per-row reset with accurate scope and sync indicators | Immutable defaults baseline, side effect dispatch, disk persistence, sync engine | Reopening preserves values; visible error states; zero credentials in reset |
| 2A — Experimentation | In-place live previews and contextual shortcuts | Staging layer, back navigation stack, texture lifecycle, side effect rollback | Cancel restores state without disk writes or sync; validated performance |
| 2B — Tailored Presets | Time-bounded trials and local performance presets | Coordination with `seguro.c`, platform bounds, batch snapshotting | Clean rollback on timeout/crash/reboot; never commit unconfirmed drafts |
| 3 — Need-Driven Extensions | Timestamped history, category-level reset, asset thumbnails, setup wizard | New versioned persistence, profile boundaries, user study evidence | Solves measured user friction; does not substitute for Phase 1-2 fixes |

Do not mischaracterize Phase 1 as "table-only changes": search requires app routing; diffs require immutable baselines; resets invoke side effects and sync. Preserving IDs, keys, and enum indices ensures binary and storage compatibility, but logic modifications are substantial.

Instrumented telemetry accompanies each delivered path. New preferences must migrate cleanly without corrupting existing configs across webOS and Tizen.

---

## 6. Architecture Risks and Invariants

- **Positional parallel arrays.** `OPCOES[]`, `CHAVE[]`, and `valor[]` are indexed directly by `OpcaoId`; new options append strictly to the end. Reordering layout in `TELA[]` does not reorder the enum. Preserve on-disk `V_*` enum integers; never insert values in the middle.
- **Composite options.** Merging external metadata and Cinemeta is a presentation abstraction; it continues writing both distinct underlying keys. Never silently normalize legacy values on screen load.
- **Account and web app sync.** `ajustes_mesclar_blob()` overwrites only existing, exportable keys. Reverting a sync-eligible setting propagates across profile devices upon server commit. Surface sync scope before action and status afterward; never overwrite defaults with account data silently. Performance presets must use local-only keys.
- **User profiles.** `ajustes-p<N>.txt` stores only the `dePerfil()` subset. Isolate device-wide and profile-specific provenance; discard pending drafts before profile switches. Sign-out clears account data while preserving device configuration.
- **Screen coverage validation.** Enforce strict 1:1 ID mapping via `conferirTela()` and `tests/ajustes_secoes.sh`. Shortcuts do not duplicate definitions; platform filters must never break search deep-links.
- **Navigation regression tests.** Simultaneously update `ajustes_secoes`, `ajustes_shot`, `notas_ajustes_shot`, deep-link helpers `ajustes_abrir_no_*`, and release cards.
- **Memory limits on low-tier hardware.** Never allocate a secondary framebuffer copy for previews; an uncompressed 1080p RGBA texture consumes ~8 MB before driver overhead. Measure allocation, frame times, and texture lifecycles directly on device.
- **Tizen WebAssembly persistence.** New state must route through `dados_gravar`, with defined schemas, scopes, and error handling. Verify persistence across process terminations.
- **Typography and accessibility.** Translate copy across all supported locales; test long strings, plural forms, glyph fallbacks, and RTL. Preserve contrast, multi-attribute focus cues, and reduced motion modes.
- **Safe mode interoperability.** Reversible trials must not exhaust `seguro.c` journal capacity (12 changes max). Coordinate interactive timers, crash detection, and rollback queues.
- **Concurrency.** If an external account sync, diagnostic run, or safe-mode rollback mutates a key during local editing, invalidate the draft candidate and display the new state. Canceling an edit must never overwrite subsequent external updates.

---

## 7. Usability Validation Protocol

Benchmark target version against the interactive prototype using identical devices, test profiles, and initial state. Alternate presentation order across participants to control for learning bias. Sample size: 5–8 diverse users.

| Task Prompt (no hints) | Evaluated Behavior |
|---|---|
| "Disable trailers when resting on poster, but keep the top hero trailer active." | Locates exact setting without altering sibling toggles |
| "Prefer Portuguese audio while keeping the interface in English." | Distinguishes interface, audio, subtitle, and metadata language settings |
| "Fix an advanced option that does not appear in the default list." | Discovers via search, inspects state, returns cleanly to query |
| "Enable this feature, which is currently unavailable." | Understands prerequisite or hardware limitation; navigates to next step |
| "Preview a different theme, then revert to original appearance." | Previews and cancels without losing previous configuration |
| "Reset an option that is also shared with another TV." | Identifies target default, profile sync scope, and pending sync status |
| "Optimize this TV for performance without affecting other devices." | Reviews local preset batch, confirms deliberately, retains ability to undo |

Record unassisted success rate, task duration, keystroke count, navigation errors, unintended setting mutations, and participant understanding of setting scope. Ask "what do you expect to happen?" prior to confirmation. Repeat test scenarios without voice input, without network access, and with advanced settings collapsed.

**Mandatory Pass Criteria:**

- Zero settings mutated purely by navigation; canceling never writes to disk or triggers network sync.
- Zero trapped focus states; Back consistently restores expected origin.
- Confirmed changes persist across app relaunch; disk write errors surface visibly.
- Offline/pending changes never display as synced; secondary profiles never inherit local settings improperly.
- Reversible trials reliably roll back on cancel, timeout, crash, and reboot; concurrent mutations follow §6 invariants.
- Search resolves correct option ID, including advanced, disabled, and localized strings.
- 10-foot legibility on 720p/1080p displays, high-contrast themes, extended translations, and reduced motion; zero text truncated to fit column width.

---

## 8. Technical Traceability

Code inspection based on commit `399c1953` (worktree `9ce4f55e`):

| Area | Source File & Symbols |
|---|---|
| Device scope and profile persistence | `src/ajustes.c`: `somenteDesteAparelho()`, `dePerfil()`, `ajustes_perfil_guardar()` |
| Blob export and sync filtering | `src/ajustes.c`: `ajustes_mesclar_blob()`; `src/sync.c`: `empurrarAjustes()` |
| Direct value mutation side effects | `src/ajustes.c`: `mudarValorDireto()` |
| Monitored high-risk settings | `src/ajustes.c`: `RISCOS[]` |
| Stability timers and crash recovery | `src/seguro.h`, `src/seguro.c`: `seguro_iniciar()`, `seguro_mudou()` |
| TV hardware resource budgeting | `src/perfiltv.h`: `PtvPerfil`; `src/perfiltv.c` |
| Spotlight routing and app dispatch | `src/spotlight.c/.h`; `src/app.c` |
| Credential masking and log filtering | `src/ajustes.c`: poster provider storage routines |

Inventory script for auditing enums across revisions:

```bash
python3 - <<'PYCODE'
import collections, re, subprocess
for ref in ('399c1953', 'be9e413e'):
    src = subprocess.check_output(
        ['git', 'show', ref + ':src/ajustes.c'], text=True)
    src = re.sub(r'//[^\n]*|/\*.*?\*/', '', src, flags=re.S)
    enum = src.split('typedef enum {', 1)[1].split('} OpcaoId;', 1)[0]
    ids = re.findall(r'\bAJ_[A-Z0-9_]+\b', enum)
    ids.remove('AJ_N')
    opts = src.split('static const Opcao OPCOES[AJ_N] = {', 1)[1].split('\n};', 1)[0]
    kinds = collections.Counter(re.findall(r'\b(ESC|NUM|LER|ACAO)\(', opts))
    print(ref, len(ids), dict(kinds))
PYCODE
```

Baseline coverage confirmed: 176 unique IDs, 0 missing, 0 duplicates, 116 basic / 60 advanced. Implementation test harnesses automate this validation for all 182 IDs across all target platforms.
