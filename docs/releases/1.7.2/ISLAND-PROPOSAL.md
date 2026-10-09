# Clock Island — Inventory and Proposal

**Subsequent owner decision:** mockup rejected. The four evolutions below are not in integration queue. Preserved as inventory/history; existing island and its identity bugfix remain. Continuity tracked in [general roadmap](../../ROADMAP.md).

Local UX proposal requested by owner on 10/02/2026. Inventory below was made on release 1.7.2 `fc711f6b` (line references from this snapshot). Identity correction integrated subsequently in `b71471bc`. This document and mockup do not alter runtime or packages. Package batch `fc711f6b` became intermediate during final card isolation review described below. Zero TVs were controlled or used as performance evidence.

The recommendation is to make the island the session continuity focal point: display when resume is ready, keep this action stable, and offer context only upon expansion. The retention limit remains **two minutes**.

## Existing Functionality

| Input / State | Origin & Inflow | Presentation & Output | Code Evidence |
|---|---|---|---|
| Clock | Device local time in `HH:MM`; polled at most once per second | Pill 52 px height; minute change transition 320 ms | `src/ilha.c:342`, `src/ilha.c:398`, `src/layout.h:600` |
| Short Notice | Key, type, icon, text, duration, and shortcut hint | One active and four queued; repeated key replaces/renews; full queue drops oldest; default 4 s | `src/ilha.c:97`, `src/ilha.c:124`, `src/ilha.h:28` |
| Notice Colors | Types INFO, OK, ERRO, and ACENTO | Success green, error coral, others follow accent. Type does not alter queue priority | `src/ilha.c:329` |
| Notice Center | Unread notice count; recommendations, schedule, updates, crash events, and other types belong to center | "N new notice(s)" and shortcut; opening center clears key from island | `src/avisos.c:797`, `src/avisos.c:847`, `src/avisos.c:865` |
| Channel Reminder | Phrase and bell icon | Outside player routes through island; over player uses dedicated pill | `src/guialembrete.c:131` |
| Activity | Text and progress 0–1 or indeterminate; refreshed every frame | Subtle dot; if number present, percentage and progress bar; fades out after 400 ms without refresh | `src/ilha.c:151`, `src/ilha.c:299`, `src/ilha.c:424` |
| Background Update | Download/install state and percentage when update card closed | "Downloading update…"; on completion, 7 s notice to restart app | `src/atualizacao.c:1093` |
| Channel List | `guia_atualizando_lista()` | "Updating channel list…"; shares activity slot | `src/app.c:3697` |
| Account Unavailable | `sync_addons_fora()`: cached copy available or none | One notice per outage, 9 s; explains use of saved add-ons or waiting for server; dismissed when restored | `src/app.c:2003`, `src/sync.h:178` |
| VOD Resume Card | IMDb, movie/show, season/episode, name, plot, poster, logo, artwork, progress, and remaining minutes | Same rule as Home: between 1% and completion threshold. Single card, no dedicated persistence; dismissed after 30 min idle, replacement session, or Close | `src/ilhacart.c:46`, `src/ilhacart.c:105`, `src/home.c:5271` |
| Premiere Card | Most recent unread schedule notice; title/episode, artwork, and "today" | Polled locally every 1 s; opening title marks read; Mark as watched dismisses card/notice without marking episode watched | `src/ilhacart.c:79`, `src/ilhacart.c:105`, `src/ilhacart.c:72` |
| Card Alternation | Both resume and premiere present | Alternates every 6 s, only when clock allowed on screen | `src/ilha.c:170`, `src/ilha.h:73` |
| Expanded Modal | Frozen copy of selected card upon opening | Artwork 480×270, logo/title, episode, short plot, and progress; Resume/Watch, Details, Close/Mark watched; transition to Saved | `src/ilha.c:200`, `src/ilha.c:535`, `src/ilha.c:551` |
| Exit Media | Interrupted eligible session and "On exit player" preference on Home | Closes detail/menu without concurrent animation and lands frame on mini poster | `src/app.c:3219`, `src/ilha.c:658` |
| Backend Retention | Confirmed pause of valid VOD; origin account, profile, URL, and episode | Resumes without new load/seek if identity still matches. Discarded after 120 s, profile/account/video switch, lost pause, or backend failure | `src/player.c:253`, `src/player.c:1541`, `src/player.c:1586`, `src/player.c:1601` |

**Real priority:** notice > activity > card > clock (`src/ilha.c:806`). No arbitration between activities: they share one slot, and the latest refresh determines content. Guide updates text in app render loop and can overwrite background update message. Transient notices may display with clock disabled; cards require clock permitted.

Clock/cards are allowed on Home when ready and without obscuring layers (`src/app.c:3583`). Notices/activity use broader guards: user within app, outside login/profile/registration, and **outside player** (`src/app.c:3693`). The island is not the video OSD.

### Existing Interactions

- On clear Home, BLUE or CH+ with card opens modal; without card, BLUE opens Saved, expanding from pill when present. 400 ms debounce avoids remote repeat (`src/app.c:1531`).
- Modal: left/right selects buttons, OK triggers, Back collapses; right past last button or BLUE again navigates to Saved. Action delivers copy of same title/episode (`src/ilha.c:219`, `src/ilha.c:244`). Modal **already freezes its selection**, even if background card alternates.
- Magic Remote: click pill opens, hover focuses buttons, click outside collapses. Pointer routing also handles touch when available (`src/ilha.c:274`, `src/ilha.c:624`, `src/ilha.c:919`).
- Resume attempts retained session first; if unavailable, opens title/episode via standard path. Close also drops retained backend (`src/app.c:2273`, `src/app.c:2287`).
- Saved expands from rect measured by island/modal; during expansion island measures but suspends rendering. Zero overlapping panels (`src/app.c:2261`, `src/ilha.c:181`, `src/ilha.c:894`).

## Existing Adaptation

| Dimension | Implemented Behavior | Evolution Implication |
|---|---|---|
| Resolution | Logical canvas 1920×1080; renderer receives actual target size. Pill 52/64 px, notice text truncated after 760 px; modal 1120×414, bounded by margins | No mobile layout in island. Proposal uses same scaled canvas; 720p readability requires TV verification |
| Position | Default Home uses left content column; Dynamic layout uses top right. Left/Right/Auto setting. Left on Dynamic sits beside menu. Guide anchors notices to right | Preserve single anchor point and expand inward, without covering menu pill (`src/ilha.c:288`, `src/ilha.c:840`) |
| Theme | Accent and tint from Settings. Glass is simple translucent surface; solid option uses dark color | Avoid adding blur. Current island glass does not capture background FBO (`src/gfx.c:1886`) |
| Motion | Pill spring `w=10,z=.72`; modal `w=7.5,z=.8`. Text switch via opacity, text does not bounce. Reduced motion disables flight/springs | Preserve motion preference, instant focus, and interruptible transitions (`src/ilha.c:310`) |
| VOD Flight | 560 ms, critically damped curve; aspect ratio preserved, transforms into poster only at end. Local artwork/source and cover; surface pulse 320 ms without shaking text | Zero frame capture, framebuffer copy, or new image allocations per frame (`src/ilha_voo.h:19`, `src/ilha.c:740`) |
| LG webOS | Hardware video plane; flight uses still/background already available | Exit image can represent work rather than exact frame. C9 is budget baseline, not evidence of new FPS |
| Android | When retained, dissolves from paused video plane in 150 ms. PixelCopy previously took 603–724 ms on TCL, so unused in this path | Use existing prepared assets; do not make snapshot exit requirement (`src/ilha.c:86`, `src/app.c:3233`) |
| Samsung WGT | Visual shortcut represents CH+ rocker. C/WASM runtime shares renderer/island | New features must continue using existing video/event contracts; browser mockup does not prove WGT performance |
| Samsung TPK | Identical native core and rendering; current shortcut helper branch uses generic blue key | Does not assume new .NET host features or alter capabilities. Adapting remote icon to target is a visual refinement, separate from pipeline |
| Remote Input | Android displays CH+, WGT CH+ rocker, others blue key; card selection and three actions via horizontal navigation | Do not depend on colored buttons on monochromatic remotes (`src/salvosintro.c:170`) |

Differing resolution islands in mockup are identical logical geometry scaled. Zero active playback, AVPlay, uMS, or ExoPlayer in HTML.

## Four Recommended Evolutions

| Priority | Proposal | Reused Data / Event | Benefit & Cost |
|---|---|---|---|
| 1 | **Resume with honest state and stable priority** | Confirmed retained session + movie/episode/account/profile identity; card progress | "Ready" strictly while confirmed; after 120 s transitions to "Saved position". Retained VOD takes priority over premiere. Focus/expansion freezes alternation. Inexpensive local check; do not hold decoder past 2 min |
| 2 | **Subtle local/offline pending indicator** | Outage detected and durable add-on queue from 1.7.2 | Small symbol, only on failure with pending edits; explanation upon expansion. Do not show success before ACK. Requires read-only query of profile queue, currently private in `sync.c:235`; zero extra polling |
| 3 | **Preparation after confirming Watch** | Source query started, stages, and cancellation in title pipeline | "Preparing movie" tracks progress without trapping user in giant loader; Cancel / Choose source reuse existing surfaces. No fake percentages, hover probes, or extra debrid GETs |
| 4 | **Miniature channel controls** | `player_mini_ativo()`, fixed channel identity, loaded program, restore/Guide/close | Single entry to Return to channel, Guide, and Close mini player. Shares pipeline/decoder; do not attempt retained VOD and live channel concurrently (`src/player.h:138`, `src/player.c:1732`) |

First evolution is most valuable and should precede others. Second leverages persistence fix already in place. Preparation and PiP require explicit event/action contracts before entering runtime to prevent race conditions.

### Proposal Product Rules

1. Single session owns island, single primary action. Notices inform without stealing focus. Session-affecting errors take precedence; routine success does not replace it.
2. "Ready" state derives from confirmed identity and pause, never card existence alone. Remaining retention time may appear on expansion if necessary, without frantic countdown at rest.
3. Retained movie does not alternate with premiere every 6 s. Focus/expansion preserves card and action; other entries accessible following primary action.
4. At 120 s backend is freed and card retains strictly position/progress. Copy does not promise instant return. Existing 30 min idle card rule remains independent.
5. Switched account/profile must invalidate card as well as backend. Implemented in `b71471bc` after inventory confirmed callchain leak; focused ASan/UBSan regression and independent review passed. Not physical TV leak reproduction.
6. Island does not steal screen space from player, keyboard, or QR. Must withdraw layer before those surfaces. Concurrent activities arbitrated by session/type, not call order.
7. Never expose URLs, credentials, account names, LAN addresses, or pairing codes in compact state.

### Ideas Considered and Deferred

- **Phone Typing:** Useful as "Text received" confirmation bound to field owner after closing existing panel. Avoid permanent QR/code in island or second input session. `celb_aberto/dono/pegar` and `celular_estado` APIs provide context; not proposed for this release (`src/celbotao.h:45`, `src/celular.h:53`).
- **Search / Spotlight:** Could open contextually when no active session, but do not duplicate search bar or compete with Saved. Spotlight is its own surface (`src/spotlight.h`).
- **Next Episode:** Only if post-player already validated next episode; avoid initiating queries/precache upon revealing island. Candidate after resume stabilization.
- **Network/FPS/codecs, social feed, badge showers, news, timers:** Deprioritized. Increase clutter or cost and demand external data; island should assist viewing, not become a diagnostics panel.

## Delivered Mockup

Standalone file, embedded assets, zero network dependencies:

`/Users/hrocha/.codex/visualizations/2026/10/02/01a0fcd2-c8be-7d62-9157-c53cb050229f/ilha/index.html`

Local preview while server active: `http://127.0.0.1:4187/index.html`. Also opens directly via `file://`. Illustrative images reuse local assets from Settings prototype; zero account/TV data.

- Home with compact and expanded island.
- Scenes: ready resume, post-2min, offline account, preparation, mini channel, and premiere.
- TV frame external controls: arrows/OK/Back, CH+/BLUE, platform selection, solid surface, and reduced motion.
- Keyboard: arrows, Enter, Esc, and C (shortcut); action focus changes without animation. Simulated flight uses 560 ms with transform/opacity; no backdrop-filter.
- Compact proposal organizes title and metadata into two lines, 84 px height; modal 432 px to accommodate pending status. These sizes **differ from current 52/64/414 px** and require TV evaluation before implementation.
- Resume and Return to Home simulate visual continuity. Details/Saved/Sources/Guide announce existing targets without navigating to unimplemented mockup screens.

Screenshots in same directory:

| File | Content |
|---|---|
| `ilha-home-compacta.png` | Home, compact card, and ready state |
| `ilha-home-expandida.png` | Artwork, confirmed state, progress, and focused primary action |
| `ilha-ponto-salvo.png` | Post-retention window; no ready resume promise |
| `ilha-conta-offline.png` | Pending add-on explanation upon expansion |
| `ilha-preparando.png` | Preparation without fictitious percentage, choice/cancel actions |
| `ilha-canal-miniatura.png` | Control card and PiP channel |
| `ilha-prototipo-controles.png` | Complete view with external scenes/controls panel |

### Completed Verification

Playwright/Chrome locally, direct `file://` load: zero JS errors and zero external HTTP requests. Horizontal navigation toggles actions, Esc collapses, C opens; Resume opens player simulation; exit completes flight animation. Reduced motion skips flight animation; 600 px viewport produces zero horizontal scroll. `QA.json` report and reproducible `capture.cjs` script in artifact directory. Two screenshots visually inspected: expanded Home and offline. Screenshots and tests are **host mockups**, without owner visual approval or TV hardware performance verification.

Before integrating any evolution: validate language/sizing with owner; test identity after account/profile switch, 120 s expiration, lost pause ACK, notice/activity concurrency, fields/QR, remote without CH+, and PiP; compare native screenshots on C9/Samsung/Android. Zero backend migration, new network calls per frame, or retention window increases.

## Post-Inventory Bugfix

Card identity bug fixed in `b71471bc`, without implementing the four proposed evolutions. Validates account/profile before events/updates and instance before fallback; clears stale card, modal, request, artwork, and flight animation, including post-idle. Notices/premieres preserved. ASan/UBSan fixture and independent review passed; new batch generated from identical source and passed on all three platforms and checksums. Physical validation remains pending. HTML/screenshots remain a visual proposal and were not altered by this bugfix. Details and regression command in LOCAL-VALIDATION.md.
