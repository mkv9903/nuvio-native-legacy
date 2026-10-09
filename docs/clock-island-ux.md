# Clock Island and Miniplayer: UX Proposal

**Status:** Proposal for review; no product changes implemented by this document.
**Scope:** Static inspection of native code in this branch and public platform guidelines, as of 10/02/2026. Not TV hardware validation.

## Product Decision

Maintain the island as a compact surface for state and resumption. In just a few seconds and without opening another screen, it must answer: "what is happening?" and "what is the next action?". The clock is neutral state; notices, ongoing tasks, and resume cards temporarily occupy the same surface. Expanding reveals details and actions directly related to the visible card. The island must not become a bloated mini-dashboard for shortcuts, recommendations, and account data.

Treat iPhone Dynamic Island as a benchmark for **visual continuity and smooth transition between compact and expanded states**, not as a literal model for size or touch gestures. A TV is shared, viewed from across the room, and navigated via directional pad. Android TV guidelines identify ~3 meters (10 feet) as typical viewing distance and recommend minimal text and cognitive overhead; Apple tvOS guidelines emphasize legible imagery at a distance, distinct focus indicators, and natural remote control interactions. [Apple: Designing for tvOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-tvos/), [Android: Design for TV](https://developer.android.com/design/ui/tv/guides/foundations/design-for-tv).

## Existing Implementation in Code

| State or Capability | Branch Evidence | Observed Constraint |
|---|---|---|
| Discrete clock | `ilha.c` renders `HH:MM`; `relogioCabe()` permits island primarily on ready Home screen without obscuring layers. Supports automatic/left/right positioning and TV Guide exemption. | Persistent clock depends on Settings and current screen permitting its presence. Notices can appear outside Home, but never over active player. |
| Brief notices | `ilha_avisar()` retains active notice and queue of four; matching key updates in-place. Validity begins on first visible frame. | Queue drops oldest notice when full. An incoming notice can preempt current activity and cards. |
| Transient activity | `ilha_atividade()` renews per frame and expires after ~400 ms without updates. Can display numerical progress. | Temporary channel without on-island history. Continuously renewed task monopolizes primary slot. |
| Interrupted session card | `ilhacart_player_saiu()` creates card with title, episode, art, progress, and remaining minutes. Only exists if returning to item is valid. Expires after 30 min of inactivity. | Does not retain background video or audio; purely a visual reminder with action to resume. |
| Premiere reminder | Pending premiere can occupy card provided clock is enabled. Navigating to title page marks reminder as seen. | Tied to clock enablement; not a general notification center. |
| Two-card alternation | Session and premiere alternate automatically every six seconds. | No compact card switcher or explicit indicator that another card is queued. |
| Card modal | Blue/CH+ button or pill click expands panel; options include Resume/Watch, Details, and Dismiss/Mark as watched. Arrows move focus; Back collapses. "Saved" sits past last right button. | Blue and CH+ are contextual shortcuts, not universally evident. Modal rendering lacks native screen reader semantics. |
| Minimize session to island | On premature exit, with clock enabled and option set, player terminates, Home restores, and art smoothly scales down into card cover. Reduced motion uses instant cut. | Animated frame is a static image (still/backdrop), not live video. LG video plane cannot be read back by OpenGL renderer. |
| Visual overhead | Lightweight pill rendering; flight transition reuses existing cached textures without extra fullscreen FBOs. | Overhead and stability across other GPUs, resolutions, scales, and SDL builds remain unmeasured here. |

Code references: [ilha.h](../src/ilha.h), [ilha.c](../src/ilha.c), [ilhacart.c](../src/ilhacart.c), [app.c](../src/app.c), [ajustes.c](../src/ajustes.c), [layout.h](../src/layout.h), [relogio.c](../src/relogio.c). The interpolated playback clock in `relogio.c` is a video time synchronizer; not the civil clock shown by the island.

## Proposed Hierarchy

Utilize four distinct levels with a single core question per level:

1. **Compact pill — Awareness.** Display time when no active state exists. For persistent card, show concise title identification and state ("Resume · S1E3 · 32 min") with visual cue that action is available. For tasks, show verb + state/progress. For notices, display short phrase and icon; omit promotional taglines or secondary topics.
2. **Focused entry — Recognition.** Using remote control, user must be able to reach pill and observe explicit focus state before activating. Focus must never trigger automatic playback. Compact pill never attempts to squeeze title, episode, synopsis, progress, and actions into single view.
3. **Expanded panel — Decision.** For playback session, display poster/still, title, episode, resume position, remaining runtime, and Resume, Details, and Dismiss actions. For premiere, show episode, air date, and Watch/Details/Mark as watched. Explain prerequisite failures; prompt confirmation only when consequences are irreversible.
4. **Destination — Completion.** Resume launches exact item and valid timestamp; Details navigates to title page; Saved opens watchlist; Dismiss removes only that card. Navigating back preserves previous screen, section, and focus point whenever destination does not replace current stack.

Content must persist across presentation changes: clock and title should not vanish and re-render without cause during expand/collapse transitions. Apple Live Activities guidelines advocate compact, high-value information, element continuity across layouts, minimal controls, and brief transitions. These serve as design principles; ActivityKit/Dynamic Island constraints do not apply to Nuvio's C renderer. [Apple: Live Activities](https://developer.apple.com/design/human-interface-guidelines/live-activities/).

## State and Interruption Model

Current codebase prioritizes compact display as: notice > recent activity > persistent card (if island fits) > clock. The proposal makes policy explicit and ensures background jobs do not indefinitely mask actionable alerts:

| Proposed Priority | Example | Behavior |
|---|---|---|
| 1. Immediate attention required | Recoverable playback error, required confirmation | Brief interruption; message outlines problem and actionable next step. Never open modal unprompted. |
| 2. Interrupted session | Player closed mid-stream | Persists as primary card until acted upon, superseded by newer session, or expired. Updates without state changes do not re-trigger animations. |
| 3. User-initiated task progress | Background update in progress, requested sync | Displays while active; upon completion, converts to single brief confirmation. Passive background work must not claim pill via artificial heartbeat renewal. |
| 4. Future reminder | Upcoming premiere | Accessible as secondary card in expanded panel; does not contest resume slot via auto-alternation. |
| 5. Neutral baseline | Clock | Stable resting state when no actionable task exists. |

If two persistent cards coexist, display them together only in the expanded view, in at most two selectable rows with stable order: interrupted session first, reminders second. Eliminate automatic 6-second rotation: it changes content under the user's gaze, degrades legibility, disorients navigation, and provides no clear mechanism to recover a vanished card. If compact footprint cannot accommodate secondary indicator without sacrificing clarity, maintain a single card and present full list upon expansion.

Each operation follows an explicit lifecycle: `inactive → in_progress → completed | failed | canceled`. Progress percentage updates adjust progress bar, but must not spawn repeated completion notifications. If higher-priority activity arrives, resume card remains staged; once activity clears, card returns without resetting context. Transient errors clear from compact view after a brief timeout, but remain accessible on relevant screens. Notice center maintains its own queue; island must not imply persistent history that engine does not retain.

## Remote Control Interactions

Apple guidelines recommend consistency with established focus and gesture patterns on tvOS; Android TV mandates that all interactive elements be D-pad navigable with predictable directional flow and clear back navigation. [Apple: Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/), [Apple: Remotes](https://developer.apple.com/design/human-interface-guidelines/remotes/), [Android: Navigation on TV](https://developer.android.com/design/ui/tv/guides/foundations/navigation-on-tv).

Proposed interaction contract, subject to verification on physical remote mappings:

| Context | D-pad / OK | Back |
|---|---|---|
| Home, collapsed pill | Directional keys reach pill when available; focus outlines element without triggering action. OK expands active state details. | Must not be the only way to locate pill. |
| Card panel | Up/Down navigate actions and cards; Left/Right maintain consistent spatial adjacency; OK activates focused action only. | Dismisses panel and restores Home with prior focus point. |
| Active playback | Never superimpose island over player while video is playing. | Back handles player exit normally; island only receives session after playback terminates. |
| Minimizing animation | Ignore second input to launch destination until landing animation finishes. | If source screen becomes invalid or enters another layer, cancel animation and snap to consistent end state. |

Blue/CH+ shortcuts can remain optional accelerators after confirming they do not conflict with TV firmware and that 100% of functionality is accessible via standard D-pad. Currently, CH+ opens modal when card exists; Blue button toggles between card modal and Saved depending on hold duration. This 400 ms hold gesture requires visual cues and testing on each remote model. Essential workflows must never rely on colored buttons, firmware key-repeat quirks, Magic Remote pointer clicks, or long-pressing OK.

Under reduced motion, snap immediately to the target state. Focus must land reliably on destination card; animations must never be a gating prerequisite for user action. App must buffer keystrokes during structural layout shifts or dispatch them deterministically afterwards without double-triggering actions.

## Island Content Boundaries

**Permitted in Compact View:** Clock; state in few words; progress indicator; small thumbnail poster only when aiding title recognition. One data point at a time beyond title. Time/progress strings must survive large font scaling, long localized translations, and high-contrast modes.

**Permitted in Expanded Panel:** Title and episode number; artwork in dedicated quadrant; resume position, percentage, and remaining runtime; up to three unambiguous primary actions; maximum two persistent cards; actionable error context. Lengthy synopses should be truncated or omitted. Profile names, account tokens, identifiers, server hosts, URLs, or API keys must never appear.

**Excluded from Island:** Continuous transport scrubbing (seek, volume, audio track selection), text keyboards, login credentials, catalog search, recommendation feeds, verbose settings menus, raw diagnostic dumps, exhaustive history logs, system-level toggles, forced updater loops, fake PiP claims, and background audio playback without engine support. These features require dedicated full-screen real estate and clean navigation stacks.

A technical notice can describe a failure, but should link to the specific settings screen that addresses root cause; do not embed an entire diagnostic tool inside the island. A premiere card must never count as "watched" merely by being displayed or receiving focus. "Mark as watched" remains an explicit user action.

## Distance, Focus, and Accessibility

Compact text must be legible at a glance from across the room. Android documentation recommends larger typography scale and bounded reading chunks for 10-foot interfaces; Apple emphasizes that all interactive components be focusable with distinct visual state indicators consistent with system conventions. [Android: Typography for TV](https://developer.android.com/design/ui/tv/guides/styles/typography), [Apple: Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/).

- Restrict compact phrases to a single line and truncate with ellipses, relying on expansion to show full text; never shrink font size to accommodate long strings.
- Pair explicit verbs with state text. Color and icons enhance, but never replace "Downloading", "Error", or "Resume".
- Differentiate informational notices, success states, and errors through clear typography, icons, and contrast, never relying on hue alone.
- In expanded panel: provide generous focus targets, ample margins, and unabbreviated labels. Current layout positions buttons along card bottom; verify remote focus travel, title safe areas, overscan, and 10-foot legibility across 720p, 1080p, and 4K displays.
- Honor reduced motion settings without stripping away vital context. Avoid pulsing glows or repetitive looping animations as the sole mechanism for conveying urgency.
- Static code inspection reveals no accessibility tree or semantic labels for the custom-drawn GLES island. Without this layer, TV screen readers cannot announce time, status, progress, or buttons. Providing accessible labels requires target-specific platform integrations across Tizen, Android, Apple TV, or SDL.

## Privacy and Shared Displays

TVs are shared communal screens. The pill can automatically display title, episode, backdrop art, progress bar, and remaining runtime from an interrupted stream; the expanded panel exposes full plot synopses. Treat this as public-facing information visible to everyone in the room, even if it contains no credentials. Official Android TV guidelines emphasize the communal nature of the living room and the need to protect personal viewing habits; Apple similarly advises against surfacing private data on ambient surfaces. [Android: Design for TV](https://developer.android.com/design/ui/tv/guides/foundations/design-for-tv), [Apple: Live Activities](https://developer.apple.com/design/human-interface-guidelines/live-activities/).

Proposal: default to neutral clock until user explicitly exits player; suppress synopses in compact view; evaluate a local setting "Hide resume details" that displays a generic "Paused video" placeholder alongside the Resume button. Introduce no telemetry, tracking, or unexpected local persistence. Cards exist only in runtime memory for the active session, governed by existing expiration rules; expanding persistence requires clear profile-switch and logout eviction policies.

## Platform, Video, and Rendering Budgets

The current minimization animation is not true Picture-in-Picture: the hardware player exits, Home renders, and a static texture scales down into the card frame. This architectural constraint stems from the LG video pipeline described in `ilha.c`; do not advertise "continue watching in thumbnail" as live video. Audio also stops immediately when `player_encerrar()` executes. True Picture-in-Picture, background audio, separate video planes, and hardware compositor integration require proven platform APIs, dedicated focus handling, and customized Back button lifecycles.

Preserve existing low-overhead principles pending new hardware benchmarks: zero GPU frame readbacks, zero fullscreen FBO passes, reuse existing texture cache, and limit flight animations to two simple quad transforms. Display a neutral placeholder if art is not already cached; never launch background network requests solely to animate an exit transition. While C9/Mali-G71 hardware represents a performance baseline referenced in code comments, it does not represent measured output for this proposal. Benchmark texture memory, dropped frames, and modal close latency across each target platform.

## Test Matrix and Edge Cases

| Scenario | Expected Proposed Result |
|---|---|
| Interrupted session with clock active | Player terminates; Home surfaces; optional art-to-card flight animation; focus rests on actionable element. |
| Clock disabled or location unavailable | Exits through standard title details page; no flight animation toward invisible coordinates. |
| Finished media playback | No resume card spawned; advances through normal next-episode progression. |
| Card expires while expansion panel is open | Panel collapses or updates with brief notification and valid focus recovery; no dangling actions pointing to dead session. |
| New session interruption arrives with one already pending | New session becomes primary; explicitly define whether previous item is replaced or queued without hidden rotation. |
| Premiere and resume card coexist | Both remain accessible without unprompted cycling; acting on one does not dismiss the other. |
| Notice or error arrives while modal is open | Does not steal remote focus or switch card underneath user. Surfaces after modal closes, unless critical system error. |
| Network disconnects or art fails to download | State and buttons remain functional with placeholder graphics; never leave panel empty or block Resume action. |
| Navigation away during flight animation / modal open | Cleanly terminates or completes deterministically without rendering stray artifacts over destination or locking input. |
| Profile switch or user sign-out | Purges cards associated with prior user profile according to session boundary rules. Verify on TV hardware. |
| Reduced motion, low framerate, or resume from system pause | Snaps directly to final resting state in single frame; never allows large elapsed `dt` to cause runaway physics glitches. |
| Large fonts / long localization strings / RTL | Layout reflows gracefully without clipping buttons, clock, or status labels; test across all translated locales with longest strings. |

## Validation Plan

Prior to code modifications, obtain product alignment on priority weighting and multi-card presentation. Next, test four primary user workflows: noticing interrupted session; resuming exact episode; dismissing without marking as watched; discovering and opening premiere while resume card is active. Compare Home screen usability with clock enabled vs. disabled across left, center, and right alignments.

Assemble remote control testing matrix: D-pad navigation, OK, Back, CH+/Blue button shortcuts where present, hardware key-repeat rates, Magic Remote / pointer behavior, and application suspend/resume lifecycles. The test harnesses `tests/ilha2_shot.c` and `tests/ilha_shot.c` generate visual fixtures for layout review, but do not validate input latency, accessibility, physical hardware integration, video PiP, or production frame pacing.

Collect device-level telemetry: framerate during flight animations and modal transitions; peak texture and memory footprint; remote keypress latency; hit-test and action dispatch reliability; user recognition that thumbnail is static art; 10-foot legibility in standard ambient room lighting; screen reader announcements. Document TV model, OS/firmware revision, resolution, exact build commit hash, and remote control model. Never extrapolate hardware support solely from platform name or desktop previews.

### Acceptance Criteria

1. Product alignment on priority hierarchy, duration, and routing for persistent cards.
2. User can locate island and execute every action using only standard D-pad, OK, and Back buttons, with zero reliance on colored shortcuts.
3. Resume reliably triggers correct title, episode, and playback timestamp; Dismiss and Mark as watched remain distinct operations.
4. Passive background tasks never indefinitely obscure playback resumption; notifications never steal remote focus during active decision-making.
5. Legibility, reduced motion behavior, title safe areas, viewing privacy, and accessibility are validated per build and device.
6. Flight animation overhead remains fully within budget on lowest-tier supported hardware; live video playback is introduced only with validated platform API support, audio routing, input handling, and lifecycle recovery.

## Platform Reference Sources

- [Apple Human Interface Guidelines — Designing for tvOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-tvos/)
- [Apple Human Interface Guidelines — Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/)
- [Apple Human Interface Guidelines — Remotes](https://developer.apple.com/design/human-interface-guidelines/remotes/)
- [Apple Human Interface Guidelines — Live Activities](https://developer.apple.com/design/human-interface-guidelines/live-activities/)
- [Android Developers — Design for TV](https://developer.android.com/design/ui/tv/guides/foundations/design-for-tv)
- [Android Developers — Navigation on TV](https://developer.android.com/design/ui/tv/guides/foundations/navigation-on-tv)
- [Android Developers — Typography for TV](https://developer.android.com/design/ui/tv/guides/styles/typography)
