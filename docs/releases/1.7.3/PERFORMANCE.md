# Per-Platform Performance Following 1.7.3 Integration

Functional integration resides in commit `99198697`. This subsequent round separates redundant work eliminations from unmeasured physical results.

| Platform | Action and Local Evidence | Limit |
| --- | --- | --- |
| Android TV | Window application compares actual SurfaceView parameters before allocating LayoutParams or requesting layout. 1,000 identical repetitions create zero new allocations, assignments, or recreations; resize, surface switch, and failure-retry are tested on JVM. | Still requires TCL / Android TV testing with real compositor and decoder. |
| LG webOS | Existing ACB deduplication preserved and exercised via real C path: 1,001 identical requests trigger a single simulated native call; PiP / fullscreen transitions continue applying. 1080p/4K scaling and pause/screensaver also pass. | Simulated call counts do not measure physical FPS. |
| Samsung WGT | Preserved AVPlay metadata cache: 1,000 repeated menu reads do not repeat getTotalTrackInfo. Preserved existing rect deduplication and retry-on-rejection. AVPlay contract passes. | Do not reduce retries or alter heap without firmware measurements. |
| Samsung TPK | Deliberate recovery reapplies ROI because certain TVs ignore accepted calls. Deduplicating solely on absence of exceptions would break this protection; preserve behavior and measure aggregate cost. | Effective call reduction not applied; requires confirm/force protocol and physical testing. |

Android receives dedicated identity in profiles and diagnostics (`platform=android`). Preserves prior conservative budgets, RAM ceilings, manual adjustments, and rollback rules; zero aggressive automated memory/thread modifications introduced.

## Pending Hardware Validation

Use identical version/sources and 5 repetitions per scenario: cold/warm Home, continuous row navigation, open/close player, PiP / fullscreen, subtitles, and seek. Record model/firmware, RAM, build, and profile; compare p50/p95 frame times, frames exceeding 33/50 ms, player startup, artwork ready time, memory usage, and errors. Promote automated tuning solely if retests demonstrate improvement without compromising stability.

Physical LG, Android, and Samsung models were not validated in this round. Zero promises of FPS gains or playback latency improvements based on host.

Subsequent validation: JVM Android test, Diagnostics suite and LG/WGT/TPK/Android dispatch, real C pause/window LG paths, WGT AVPlay contract, and TPK metrics passed. TPK 40/60/65 hosts recompiled without errors; TPK 60/65 retain existing SDK/target warnings. TPK metrics aggregate requests/repeated/applied/failed and native session duration without per-request extra logging or allocations; 10,000 explicit requests continue applying.
