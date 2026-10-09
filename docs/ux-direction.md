# Nuvio: UX Direction, Motion, and Rendering

Review from 10/02/2026. Research linked to 1.7.1 codebase, with Settings implementation in an isolated worktree. Apple inspiration serves as a reference for clarity, continuity, and polish. The experience must work with D-pad, Magic Remote, low memory, and video on separate presentation surfaces.

## The Experience We Want to Build

The app responds to the first keypress, clarifies where the next OK will act, and maintains context when returning. Motion bridges two recognizable states. Text and focus remain sharp while art, lighting, and materials scale quality down when the device requires it.

A compelling visual signature can stem from five small, meticulously crafted interactions:

| Experience | User Perception | Implementation and Evaluation |
|---|---|---|
| Focus matching intention | Every keypress gets a response; highlight glides without leaving control behind | Immediate logical selection, replaceable animation target, no queued moves. Test rapid repetition and D-pad reversal. |
| Opening preserving context | Selected card leads smoothly into detail page, and Back restores position | Pre-warmed art, identity and focus preserved; no heavy decoding on frame 0. Staging uses generation tags and cancellation. |
| Restrained materials | Display exhibits depth and light with crisp readability | Single tonal surface and local highlight; reuse art/small FBO. Keep text and borders immune to downsampling. |
| Island as session continuity | Exiting an episode creates clear resume affordance without overtaking Home | Pill → focus → panel → destination. Resume and premier have stable order and distinct actions. Current animation uses static art; do not promise PiP video. |
| Reliable Settings | Exploration is safe, cancellation is predictable, default is clear | Local search, pending selection, OK to apply, Back to cancel, explained dependencies, and per-item reset. |

Polish also relies on what is avoided: auto-playing trailers on incidental focus; cycling cards while user is reading; shifting OK target with async responses; animating entire layout on single value change; piling on fullscreen blur, shadows, and dimming veils.

## Island: Prioritizing Relevant Action

The [island proposal](clock-island-ux.md) documents current behavior and proposed states. Recommendation: begin with clock, interrupted session, and user-initiated operations. Premieres can coexist in expanded panel. Compact view shows one primary state; panel reveals related actions.

First experiment should replace automatic card cycling with stable order, make D-pad access clear, and preserve focus upon collapse. Color keys remain optional shortcuts. Synopsis, search, account, audio/subtitle controls, and raw diagnostics belong on dedicated screens that can properly present them.

Subsequent prototype ideas: sleep timer; action results with Undo; explicitly selected next episode; requested update progress. Each concept must demonstrate why the island is the right home and account for expiration, cancellation, profile switching, and playback collision. Not implemented or approved as new features by this document.

## Unified Visual Quality, Tailored Technical Paths

| Platform | Concrete Priority | Decisive Experiment |
|---|---|---|
| Samsung `.wgt` | Browser cadence, WebAssembly runtime, explicit texture memory, and WebGL 1 compatibility | Measure `requestAnimationFrame` intervals, long tasks, and uploads. Compare low-res local material with current surface. |
| Samsung `.tpk` | Pacing between host/framework and EGL context; GPU-scaled effects | Measure context wait, draw CPU, and swap timing. Preserve single context owner; compare tiers without blaming shaders for host delays. |
| LG webOS | Legible frame 0, pre-warmed text, correct composition with decoupled video | Compare cold/warm sheet open, raster queue, and uploads; validate alpha hole and video rectangle. Art can drive material; native video is not a readable GL texture. |
| Android TV | SDL thread cadence and latency, `SurfaceView`/Media3, and existing adaptation | Capture Perfetto traces with equivalent scenes; separate app, compositor, and video. Evaluate Swappy/Choreographer only after proving current pacing is bottleneck. |

Platform specifics, official sources, and device matrices: [Samsung](ux-render-samsung.md), [LG](ux-render-lg.md), [Android](ux-render-android.md). These documents represent research and proposals; they do not claim realized performance gains on hardware.

## Recommended Execution Order

1. **Finalize interaction contract.** Review native Settings, search, back navigation, error handling, dependencies, reset, and 10-foot viewing distance. Complete localization and test with physical remotes prior to release.
2. **Measure reproducible baseline.** Same build, data, and scenes across each TV. Capture frame distributions, key latency, rasterization, uploads, and memory. Separate cold/warm open, idle, scrolling, focus reversal, and active video.
3. **Build three polish test scenes.** Focus/scroll, Settings sheet, and resume island. Each scene evaluates current vs. proposed state under normal/reduced motion, art present/missing, and long text strings.
4. **Optimize observed bottleneck.** Incremental text/art staging if CPU-bound; compact/cached materials if fillrate-bound; presentation pipeline if pacing-bound. Alter one variable per experiment and preserve visual baseline.
5. **Expand approved components.** Propagate unified design grammar across Home, detail, search, and player; avoid rewriting entire renderer solely for visual uniformity.

Frame deadline derives from measured effective cadence, not panel's marketing specs. For example, 60 fps yields ~16.67 ms: an arithmetic reference, not an automatic Nuvio delivery guarantee. Target p95/p99 thresholds and jitter tolerances must be set after baseline, leaving headroom for media and input.

## Quality Criteria

Results must remain compelling with real artwork, extended text, rapid key bursts, and active video playback. An isolated attractive screenshot proves nothing. Document TV model, firmware, package hash, physical resolution, input device, and motion preference in every test run.

Before promoting an effect, verify: first keypress still responds immediately; focus and text remain crisp; Back restores exact prior position; zero actions require waiting on animations; memory stabilizes over repeated loops; reduced motion preserves complete context. For screen readers, semantic integration is mandatory: drawing text in GLES does not automatically create an accessibility tree.

## Status of This Delivery

Implemented in code: intent-based Settings hierarchy, category-level advanced options per session, local search with query restore, navigation-only picker without auto-save, factory defaults diffing, individual item reset, and dependency alerts. Tests and captures are host-based using test fixtures.

Remaining as next deliverables: live Home preview with timed rollback, local performance presets, island refinements, and renderer optimizations. Documents outline the roadmap; these features are not yet implemented or benchmarked on physical TVs.
