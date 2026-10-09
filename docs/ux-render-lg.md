# LG webOS: Motion, Components, and Rendering

Research and proposal from **October 2, 2026**, based on checkout `2ab3ccc8` and ongoing Settings modifications. This document does not modify the renderer. The goal is a TV experience with the clarity, continuity, and polish associated with Apple, while preserving LG's unique characteristics: Magic Remote, D-pad, legacy webOS, GLES 2, and decoupled native video pipeline.

## Evidence and Scope Limits

**Verified in this research:** analysis of C/SDL/GLES code paths and primary sources cited below. **Repository history:** comments with prior measurements on the C9, without reproduction or direct access to raw logs in this task. **Proposal:** experimental techniques, durations, and bounds identified as such. No installation, performance capture, or remote-control testing on a physical LG occurred during this research. Compiling on Mac, running the HTML prototype, or capturing a test fixture does not demonstrate TV fluidity.

The [README](../README.md) mentions 60 fps and zero jank on the C9 home screen. That record alone does not document environmental conditions, timing distributions, or performance of newer screens. The C9 baseline is critical for regression testing, but does not represent all webOS GPUs or firmware revisions. Never infer native compositor pacing solely from the panel's advertised refresh rate or an HDMI input mode.

## Preserved Architecture

| Component | Current Code Evidence | Consequence for Proposal |
|---|---|---|
| Window, drawing, and coordinates | [main.c](../src/main.c), [gl_compat.h](../src/gl_compat.h), [layout.h](../src/layout.h): SDL2, GLES 2 on TV, OpenGL 2.1 on Mac; logical canvas and physical drawable are distinct. `SDL_GL_GetDrawableSize` determines effective pixels. | Use granted dimensions and actual scale; never assume requesting 4K guarantees a 4K framebuffer. Shaders and extensions require GLES 2 fallback. |
| Video composition | [main.c](../src/main.c) requests alpha and creates a non-opaque Wayland surface. [video.c](../src/video.c) uses LS2/`com.webos.media`, `libAcbAPI` via `dlopen`, and SDL exported window path for later generations. | Video appears underneath the UI through the transparent cutout. It is not a GL texture available for blur or capture via `glReadPixels`. Respect alpha cutout, crop, rectangle, and player lifecycle. |
| Loop and frame pacing | [main.c](../src/main.c) requests swap interval 1 on TV and 0 on Mac; configuration return value is unchecked in that block. Measures phases with CPU clock, caps visual `dt` at 100 ms, and tracks jank with fixed 33 ms threshold. | Verify platform support and measure cadence. Swap wait includes synchronization and pending work; not equivalent to raw GPU time. Fixed 33 ms threshold misses dropped deadlines. |
| Animation | [anim.h](../src/anim.h): exponential approach, critically damped spring, linear ramp, and edge response; reduced-motion policy snaps to target immediately. | Reusable baseline exists. Current spring caps `dt` at 50 ms, zeroes velocity counter to new target, and prevents overshoot. Velocity continuity across reversals is not guaranteed; altering requires perceptual evaluation. |
| Primitives and effects | [gfx.c](../src/gfx.c): shape shaders, state cache, rectangle batching, separable blur, combined backgrounds, ambient lighting in small FBO with ping-pong targets. | Optimize existing paths. Do not reintroduce fullscreen passes to simulate depth that a single material surface can communicate. |
| Images | [tex_cache.c](../src/tex_cache.c), [perfiltv.c](../src/perfiltv.c): decode off render thread, upload on GL thread, layout-driven sizing, priority/LRU, and visible art pinning. `main.c` pumps up to three entries per frame. | Upload count alone does not limit total bytes or duration. A single backdrop upload can cost more than multiple posters; monitor backlog and per-resource costs. |
| Text | [text.c](../src/text.c): 512 line cache entries, family/style fallback, measurement and font selection cache, scale-aware rasterization. Four lines minimum **floor**, with 24 ms budget and maximum 64 lines per frame. | Explicit tradeoff between initial modal open spike and progressive label pop-in. Proposed solution pre-warms predictable text prior to entrance, rather than merely lowering threshold and restoring pop-in. |
| Capabilities | [gpunivel.c](../src/gpunivel.c): compiled adaptation for `NV_TPK`/`NV_ANDROID`; LG remains at level 0. [perfiltv.c](../src/perfiltv.c) sets texture budget based on system RAM. | LG does not use this adaptive policy merely because the module exists. Expanding requires a dedicated experiment; total RAM does not indicate free VRAM or GPU performance. |
| Input | [ponteiro.c](../src/ponteiro.c), [main.c](../src/main.c): targets with stable IDs, hit-test list, and SDL LG Back key handling. | Share logical state between pointer and D-pad. Do not replace native keycodes with JavaScript web documentation keycodes. |

Public LG documentation distinguishes graphics resolution from video resolution: for UHD web apps, it specifies 1920×1080 graphics and 3840×2160 video. This contextualizes surface separation; it does not set a hard ceiling for this native application's framebuffer. Accurate dimensions remain those obtained at runtime. Sources: [LG — App Resolution](https://webostv.developer.lge.com/develop/specifications/app-resolution), [SDL — GetDrawableSize](https://wiki.libsdl.org/SDL2/SDL_GL_GetDrawableSize), accessed 10/02/2026.

The ACB/Wayland paths described above are Nuvio implementation realities, not public LG API guarantees. Do not transfer webOS OSE or browser engine contracts onto these private integrations.

## Visual Direction: Unified Grammar Across Components

Focus must clearly indicate where the next OK press will act; selection, value editing, and playback remain separate states. The Apple reference separates focus and activation, recommending margin around elements for expansion. Apply this principle to cards, Settings rows, tabs, controls, and buttons, without copying tvOS pointer restrictions. Source: [Apple — Focus and selection](https://developer.apple.com/design/human-interface-guidelines/focus-and-selection/), accessed 10/02/2026.

Proposed shared contracts, subject to physical panel and distance validation:

| Component | Proposed Behavior | Cost and Fallback |
|---|---|---|
| Card | Immediate feedback; subtle scale, compact shadow, stable resting state; image preserves framing. Border must not clip at row boundary. | Transform geometry, reuse texture; on essential profile, outline/contrast without scale or wide shadow. |
| Settings row / list | Clear focus indicator and legible value; disabled dependency displays explanatory notice. Highlight travels without shifting entire layout. | Single highlight surface; do not re-rasterize text simply because row changed position. |
| Scroll | Logical selection updates on received key; presentation smoothly converges to latest target. Rapid key repetition does not build animation backlog. | Single translation and scissor clip; avoid redrawing offscreen content. |
| Sheet / modal | Snappy entrance with clear visual hierarchy; background stabilizes, focus lands on active control; Back closes modal and restores previous focus. | Pre-warmed material and veil; over video, never attempt native video plane readback. |
| Hero | Crossfade between two pre-loaded backdrops; title and actions remain crisp. Never trigger video playback from incidental focus scroll. | Reuse existing blend path; cancel asset staging for superseded targets. |
| Keyboard / search | Keystrokes and cursor receive immediate feedback ahead of remote results. Late network results must not jump focus without deterministic rules. | Stable text layout and geometry; results insert via data model updates without rebuilding all assets. |
| Player | Controls appear without altering video framing; subtitles and focus maintain visual hierarchy. | Preserve alpha cutout and media surface, using subtle local overlays. |

The animation library must track current/target positions, velocity when applicable, opacity, scale, priority, and completion criteria. Drawing consumes resolved state; never triggering network requests, disk I/O, or navigation. Interrupting a transition must never restore stale positions or invoke callbacks from dismissed screens. IDs and generation tags bind asynchronous results to their correct origin.

**Initial timing hypotheses:** focus response starts on first available frame; focus settling within 140–220 ms; sheet within 220–300 ms; hero visual swap within 280–420 ms. These are benchmark comparison ranges, not rigid Apple numbers or proven C9 limits. Test three variants within the range and record initial latency, settling, and legibility. Rapid direction reversals, edge focus, and long jumps require tailored curves; a single universal spring cannot handle all distances.

Evaluate position and velocity continuity independently: preserving velocity during reversal can cause temporary travel away from the new target; abruptly zeroing velocity can create perceived discontinuity. Primary TV requirement is predictable response to remote input. Benchmark current implementation against clamped velocity retargeting, avoiding excessive aesthetic overshoot.

### Reduced Motion and LG Input

Reuse `anim_politica_reduzida`, audit transitions outside `anim.h`, and disable parallax, zoom, breathing, and animated blur. Maintain feedback via contrast, outline, text, or subtle crossfade where it aids orientation. Apple guidelines treat these motion effects as potential triggers and recommend alternatives preserving functional context; Nuvio's local preference must operate regardless of whether system accessibility APIs are accessible on LG. Source: [Apple — Reduced Motion evaluation criteria](https://developer.apple.com/help/app-store-connect/manage-app-accessibility/reduced-motion-evaluation-criteria), accessed 10/02/2026.

LG documents pointer, click, wheel, and 5-way D-pad navigation, including seamless transitions between modes. Nuvio must validate all of them. Apple TV interaction paradigms that discard cursor functionality do not apply to the Magic Remote. Source: [LG — Magic Remote](https://webostv.developer.lge.com/develop/guides/magic-remote), accessed 10/02/2026.

In the proposed hit-testing model, intention bounds remain stable during focus scaling, while the visual bounding box tracks the animation. Specifically test boundaries between cards to prevent hover jitter caused by card expansion itself. When switching between D-pad and pointer, preserve the last valid focus and prevent residual mouse events from stealing selection. Never delay OK or Back to wait for an animation to finish.

## Renderer: Concrete Improvement Steps

### 1. Stage Work Before Transitions

Introduce explicit screen pre-warming: geometry, key text strings, and available art receive priority; secondary images remain incremental. `txt_pendentes`, `txt_rasterizadas`, `txt_despejos`, upload byte counters, and queue age reveal whether the scene is ready. Pre-warm only the most probable next screen, with generation tags, cancellation, and strict memory ceilings, avoiding whole-catalog preloading.

The current 24 ms text budget already exceeds a single 16.67 ms frame period at 60 Hz before any other work. This is an arithmetic observation from code, not proof that every modal takes 24 ms. Measure cold vs. warm opening times. Experiment: pre-warm Settings labels and modal text on font worker threads in small slices prior to transition, maintaining immediate user action and feedback. If data is still missing, present a coherent skeleton/loading state; never render focusable controls without labels.

Preserve Inter and existing fallback fonts, scale-aware rasterization, weights, and script fallbacks. Do not re-rasterize during zoom animation steps; render at texture size suited for maximum focus scale. A glyph atlas is a later investigation, dependent on proven line-cache pressure: it must support complex shaping, kerning, accents, complex scripts, weights, and fallbacks. Do not replace text rendering with SDF based on assumed savings; verify small-body legibility and total overhead first.

### 2. Separate Layers by Mutation Frequency

Proposed layers: pre-warmed background/material; visible content; focus highlight; text/icons; modal overlay; player controls and video masks. This separation represents invalidation and responsibility boundaries, not a mandate for separate FBOs per layer. Fullscreen FBOs per layer would multiply memory footprint and fillrate overhead.

Reuse separable blur and caches in `gfx.c`. Current ambient lighting uses 320×180 with ping-pong targets; a new panel can derive color/blur from static art at 160×90 or 320×180 depending on performance. Recompute only when source artwork, theme, or dimensions change, not on every cursor move. If background is native video, use dark tonal material derived from metadata or static backdrop. Real-time blur of native video would require a different compositor architecture, outside this scope.

The intended material combines localized luminance, contrast, and subtle edge lighting. It does not require per-pixel refraction or heavy translucent drop shadows. To reduce overdraw, draw opaque where semantically possible, eliminate redundant dimming veils, scissor passes to active region, and avoid multiple fullscreen blend passes when an existing shader combines sources. Fallback surfaces must preserve identical hierarchy and layout.

### 3. Minimize Draw Submissions Without Breaking Visual Ordering

Instrument draw calls, texture binds, shader program/scissor switches, covered area, and FBO passes. Experiment with batching consecutive rectangles sharing compatible texture, program, blend mode, and scissor clips, using dynamic VBOs compatible with GLES 2. Never sort transparent primitives globally by texture: this alters blending order. Flush on state changes, masks, render target switches, or ordering barriers. Compare CPU savings against vertex upload overhead; do not assume automatic gains.

Replace the rigid three-upload-per-frame limit with a composite limit considering count, bytes, and elapsed time. Prioritize essential text and focused art; discard stale requests via generation counters, pin visible sets, and constrain decoded surfaces awaiting upload. Soft deadlines cannot interrupt an in-flight `glTexImage2D`: reducing resource resolution and per-call work remains necessary.

## Memory and Capacity: Comprehensive Budget

The current image cache must not be conflated with total graphics and process memory. Build a local memory audit: GL textures, text lines, effect FBOs, decode staging, network buffers, vertex buffers, and pending deallocations. Video, driver, and compositor may introduce opaque allocations; record this gap alongside RSS and visible system pressure. `MemTotal` does not reveal free VRAM.

For scale, an uncompressed RGBA8 target without mipmaps requires approximately **7.91 MiB at 1920×1080** and **31.64 MiB at 3840×2160**; two additional 4K targets add 63.28 MiB before alignment and driver copies. A pair of 320×180 RGBA8 targets is ~0.44 MiB. These are raw dimensions × 4 bytes arithmetic, not driver reports; current runtime uses RGB and implementations allocate differently.

Initially preserve the existing lookup table in `perfiltv.c`: LG models below 1.2 GB RAM receive 48 MB cache / 64 MB cap; 1.2 to 3 GB receive 128 MB cache with distinct caps; the 2–3 GB range associated with the C9 allows up to 300 MB cap. These are existing encoded policy limits, not a recommendation to expand footprint. Historical comments note the C9 with 2245 MB RAM and a session using 35 MB across 44 textures. Validate real memory consumption on new flows before modifying budgets.

Proposed session capability detection: GL version/renderer, active extensions, max texture size, FBO support and completeness, shader precision, pixel format, drawable dimensions, RAM, and video pipeline version. Log platform capabilities separately from runtime performance. A valid FBO does not prove its rendering cost fits within frame deadlines.

| Experimental Profile | Preserved Appearance | Priority Reduction |
|---|---|---|
| Essential | Sharp text, clear focus, complete geometry/actions, instant response | Solid tonal material, no new blur, no breathing/parallax; compact cache and size-matched art. |
| Balanced | Standard layout and typography, subtle focus scale, refined depth | Small pre-warmed blur on change, contained shadows; no continuous heavy effects. |
| Full | Rich focus continuity, animated art transitions, pre-warmed materials | Enable advanced options only after measuring headroom under active video playback and memory load. |

Start with a conservative profile; add runtime adaptation with stable evaluation windows, hysteresis, and one reduction step at a time. Never fluctuate visual effects during user interaction. Distinguish user preferences from transient degradation; never overwrite user settings or sync device-specific capacity profiles. Disentangle GPU, CPU, memory, and I/O bottlenecks before degrading quality. Reducing blur will not fix network stalls; lowering resolution will not fix input queue latency.

## Frame Pacing and Instrumentation

First verify return value of `SDL_GL_SetSwapInterval(1)` and log queried interval; 1 requests synchronization, not a guaranteed refresh rate. Adaptive vsync is optional and not guaranteed on LG; do not set -1 as a blanket fix. Source: [SDL — SetSwapInterval](https://wiki.libsdl.org/SDL2/SDL_GL_SetSwapInterval), accessed 10/02/2026.

Instrument per-frame samples tracking event sequence, logical focus, feedback dispatch, CPU stages, uploads/text, and swap completion, with bounded ring buffers and asynchronous flushing. The SDL counter measures elapsed CPU time, not physical photon emission. Source: [SDL — GetPerformanceCounter](https://wiki.libsdl.org/SDL2/SDL_GetPerformanceCounter), accessed 10/02/2026.

If `GL_EXT_disjoint_timer_query` and entry points are available, use GPU query rings, reading results in subsequent frames and discarding disjoint intervals. Without this extension, label CPU metrics accurately. Never inject `glFinish` into production benchmarks to fabricate GPU metrics: it alters pipeline behavior. Source: [Khronos — EXT_disjoint_timer_query](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt), accessed 10/02/2026.

Calculate `T = 1000 / effective frequency` in ms only after verifying display cadence. For initial benchmarking, target renderer CPU p95 below `0.35T`, GPU p95 below `0.60T` where measurable, and incremental upload/text work around `0.10T`, tuning against baseline. These slices cannot simply be summed linearly: CPU and GPU overlap work, and the compositor has its own schedule. At 60 Hz these represent ~5.8 ms, 10 ms, and 1.7 ms; these are targets, not C9 achievements.

Initial interaction benchmark: logical focus on event processing, feedback submitted within two frame periods, and zero growing latency tail under sustained directional hold. This measures event arrival → submission, not remote button press → photon. For the latter, record remote control / press indicator and display with a high-speed camera, repeating at least 30 actions per scenario and reporting instrument temporal resolution. Document p50/p95, worst case, and sample size; avoid claiming precision beyond capture resolution.

The current `dt > 33 ms` counter allows reporting zero jank while still dropping frames in a 60 Hz presentation. Add frame duration histograms, counts of intervals exceeding `1.5T`/`2T`, and consecutive miss bursts, identified as main-loop approximations when compositor timestamps are unavailable. Average FPS alone cannot approve a change.

### Subsequent Experiment: Partial Redraw

If EGL and SDL on the target firmware permit, evaluate `EGL_EXT_buffer_age` with damage tracking and `EGL_KHR_swap_buffers_with_damage`. The former enables reconstructing historical backbuffer content; the latter communicates dirty regions to the compositor while maintaining backbuffer consistency. Buffer age zero, resizes, and full invalidations require complete redraw. Do not bypass SDL swap functions or maintain separate presentation queues without verifying EGL lifecycle. Sources: [Khronos — buffer age](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_buffer_age.txt), [Khronos — swap with damage](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_swap_buffers_with_damage.txt), accessed 10/02/2026.

This is a later phase: moving hero backdrops, alpha transparency, and the native video plane complicate damage bounding. Possible utility in static Settings or localized focus; verify rendering artifacts and video hole behavior before enabling.

## App Lifecycle and Resumption

LG documents lifecycle transitions between active and suspended states for web apps. The native C renderer must handle actual events received by SDL and the native shell; never assume JavaScript `visibilitychange` arrives at the C loop. Source: [LG — App Lifecycle Management](https://webostv.developer.lge.com/develop/guides/app-lifecycle-management), accessed 10/02/2026.

Proposed approach: pause cosmetic animations when hidden, throttle pending background work, invalidate stale staging queues, and restore consistent UI state upon resume. Handle suspension, focus loss via system overlays, drawable destruction, and context loss as distinct events. Reset presentation clocks without fast-forwarding accumulated seconds of backlog animation. Recreate GL resources only when destroyed, preserving cached CPU data within memory budgets. Validate Home, LG system menu, keyboard/voice inputs, video resumption, and Back button handling during modal transitions.

## Experimentation and Delivery Matrix

| Phase | Isolated Comparison | Acceptance Evidence |
|---|---|---|
| 0 — Baseline | Identical build and sequence without visual changes; cold/warm, video stopped/playing | Package hash, TV model, firmware, drawable, GL renderer, SDL version, config, and documented metrics. |
| 1 — Component contracts | Current focus/spring vs. new variant; D-pad and pointer | Slow-motion capture and standard testing, zero lost focus events, instant input response, zero regression under reduced motion. |
| 2 — Text/art staging | Current open latency vs. bounded pre-warming | Full labels rendered, open and input p95/p99 improved or equivalent, no new memory spikes or backlog. |
| 3 — Material / cache / batching | One optimization per test run | Identical framing/alpha/color reproduction, documented GPU/CPU cost, resources released upon modal exit. |
| 4 — Capability profiles | Essential / balanced / full profiles on same TV | Fallback preserves text and interactions; no visual flickering, no accidental preference writes, stable under memory pressure. |
| 5 — Compositor | Damage / pacing only if extensions and driver permit | Zero stale artifacts, unclipped video hole, no black frames, no tearing, no input degradation post-resume. |

Execute on a **physical LG C9** and at least one low-RAM LG model and one newer webOS generation. Record exact model and firmware; "webOS" alone does not specify driver behavior. C9 provides regression continuity, not universal proof. Never enable an optimization globally simply because it worked on desktop Linux.

Per-variant test plan: start with cold cache; navigate 100 focus transitions including directional reversals and repeat holds; open and close Settings and search; switch theme/font; load large catalog; stress memory with image decoding; display player controls and subtitles over active video; toggle window/hero/fullscreen where applicable; background and resume; repeat with warm cache and reduced motion. Run continuous sessions for at least 20 minutes to observe resource trends and degradation, without treating elapsed time as definitive proof against leaks.

Deliver visual captures, timing comparisons, distribution histograms, and resource counters per scenario. Mac/prototype artifacts facilitate visual and flow review; TV results require physical remote/screen recordings and matching build logs. Decisions to adopt a variant must cite measured advantages and any trade-offs. If GPU or compositor timings cannot be reliably gathered, document measurement limits and restrict conclusions to verified CPU metrics.

## Historical Notes Guiding Initial Experiments

- `gfx.c` records drops to 34 fps and worst-case 50 ms frames on the C9 with the earlier ambient lighting implementation on 09/26/2026, motivating the 320×180 FBO design. This prioritizes avoiding fullscreen fillrate regressions; not a benchmark run in this study.
- `text.c` records measurements showing the progression from two to four lines and subsequently to a 24 ms frame budget. Pre-warming must address both render spikes and visible label pop-in.
- `perfiltv.c` documents memory pressure on lower-tier LGs and clarifies where OOM terminations were hypothesized. Future plans must maintain the distinction between RSS correlation, crash causes, and confirmed system events.

Recommended first implementation following this research: minimal instrumentation, text pre-warming for Settings, and a unified focus component. Benchmark on C9 before expanding to advanced materials, batching, or adaptive scaling. Visual fidelity relies on predictable latency, stable typography, and smooth continuity; graphical embellishments should only follow validated performance headroom.
