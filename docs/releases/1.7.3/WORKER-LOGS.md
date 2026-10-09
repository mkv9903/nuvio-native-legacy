# Worker/D1 — Review of Client 1.7.3 Logs

10/03/2026. Previously authorized read-only diagnostic query: private dump of 500 records, IDs 21252–21751; query confirmed `rows_written=0` / `changes=0`. Private text remains outside repository. This report contains solely IDs, counts, and sanitized technical events. Zero changes made to runtime, server, or deployment.

## Version Identity Prior to Verdict

| Record | Uploader Version | Embedded Version / Scope | Conclusion |
|---|---|---|---|
| 21737 | Android 1.7.3 | `[tv]` identifies app 1.7.3; includes evidence of previous 1.7.2 session | Evidence of 1.7.3 execution, without attributing previous session output to current binary |
| 21732 | Android 1.7.3 | 1.7.2 execution header; four 1.7.2 tokens in body | Historical log submitted after update; does not count as 1.7.3 playback or fluidity |
| 21745 | Android 1.7.3 | Diagnostic report with `versao=1.7.3`, without FPS/playback window | Confirms emission of this report; does not validate playback |

Prior exit in 21737 mentions package installation (`installPackageLI`, status 0), not a fatal signal or OOM. This does not prove a 1.7.3 crash. The sample has only three uploads labeled 1.7.3 and none of them provide LG/Samsung 1.7.3 validation.

## Strongest Findings

### 1. ~1s Jank During App Update — Reproduction Priority

21737 records worst frame of 995.6 ms: `upd=910.1 ms`, `des=77.5 ms`, `swap=2.1 ms`, `bomb=0.0 ms`, RSS 419 MB. Dominant delay is update, not texture upload or swap wait. Surrounding context contains video, Where to Watch, and source events. Real observation of a stall, but insufficient stack trace to pinpoint responsible function.

Concrete suspects for instrumentation: `main.c:1183` wraps `app_atualizar`; `streams.c:1258` updates apps upon opening sources; `ondever_apps_atualizar` calls Android adapter; `NuvioActivity.listarApps` executes `PackageManager.queryIntentActivities` and list assembly. This synchronous enumeration warrants dedicated duration measurement and caching, but was NOT proven to have consumed the 910 ms. Also measure sync application / rebuilding rows in same update prior to selecting fix.

Reproduction: same Android TV / build, open cold and warm sources, alternate detail/back/home, and capture spans `apps-enumeration`, `streams-open`, `sync-apply`, `home-rebuild`. Criterion: confirm which call dominates update and eliminate UI blocking without publishing stale cache after installing/removing apps. Do not increase memory budgets as treatment for this event.

### 2. Fluidity Depends on Render Mode — No Universal Gain Demonstrated

21737 presents a stable sequence near 60 fps following Home load, followed by a drop in glass/layout mode; mode log displays `fps=43.3`, `layout=2`, glass enabled. The 27-sample set has median 59.3 fps and range 43.4–60.3, but mixes screens and modes: not a controlled comparative benchmark. A 132.6 ms frame also appears, dominated by 119.8 ms rendering. Measure fixed screen/mode and cost of fill, blur, and text; do not interpret session median as universal approval.

21732 contains 306 samples, median 46.75 fps, RSS 211–593 MB, but was running 1.7.2 and toggling glass/modes. This history serves to plan controlled comparisons, not to declare 1.7.3 regressed or improved. Growing RSS alone does not prove a leak; decoder, images, and cache have legitimate allocations and require repeated cycles with plateau/return.

Suspects: `gfx` / glass modes and drawing work in `main.c` instrumented by `[gpu-modos]` and `[quadro]`. Preserve budgets until measured A/B on identical screen. The Android redundant layout fix was validated on JVM, but this sample does not identify installed commit/hash and does not demonstrate hardware gains.

### 3. Trailer/Source Failures Exist in 1.7.2 Without Justifying 1.7.3 Block

21732 includes Android error 2000 event and trailer cancellation/error. The code in `NvPlayer.onPlayerError` preserves errorCodeName/code/message and emits event 5; numeric error alone does not identify expired URL, timeout, codec, or decoder. Investigate with controlled legal asset, resolution status, HTTP, and nominal error code, keeping private URLs out of shared logs.

In current 1.7.2 records, 14 TPK uploads include numeric errors `0xfe6c002e`, `0xfe6c0026`, or `0xffffffff`. The same dumps may repeat prior sessions: 59 textual occurrences do not equal 59 independent failures. Recent examples: 21739, 21722, 21704. Do not attribute these codes to codec/network without host contract; new `video_tpk.c` provides numeric evidence to error screen, but still requires source/device to prove resolution.

102 records labeled 1.7.2/1.7.3 contain curl timeout 28 (545 occurrences in aggregated text). This mixes add-ons, artwork, and other queries; does not prove general player or Worker failure. Check subsystem and cancel queries on generation change prior to adjusting deadlines. `trailer.c` / resolution and `NvPlayer.kt` are reproduction targets; slow provider networks do not justify altering global cache.

## Limits and Gates

- Dump search did not locate `SIGSEGV`, `SIGABRT`, `Fatal signal`, `ENOSPC`, or `EDQUOT` in records labeled 1.7.2/1.7.3. Absence in stored text does not prove absence of crash or full storage; dumps may be truncated (many are 204,800 characters) and contain solely the tail.
- Zero physical LG/Samsung 1.7.3 validation in this sample, zero reliable per-upload hash, and zero controlled baseline/fix pairs. Publishing must not turn diagnostic upload into blanket proof of all features.
- Live tail of Worker exceptions was unvalidated: 35s attempt produced zero output or connection marker. Do not claim Worker has zero exceptions. D1 is stored client log, not Worker runtime trace.
- Issue #223 16-line attachment was analyzed separately: 4K support return must not be converted into proof of 2s abort. See corresponding issue report.

Priority next steps: reproduce 910 ms update with spans; A/B glass/fixed screen per device; reproduce trailer 2000 / TPK codes with controlled source and redacted details. Each result must distinguish installed build, prior session, host UI/test, and hardware proof.
