# Player 1.8 — Specification and Feasibility Proofs

10/03/2026. Implementation proposal, without physical benchmark and without features implemented by this document. Scope defined by user: these improvements belong in 1.8; Settings/fixes, recovery of local work, and optional icons are handled separately in 1.7.3.

## Baseline and Conclusion

Reuse the C player, `legenda.c`, `mkvass.c`, `vazao.h`, `diagnostico.c`, `perfiltv.c`, `seekr.c`, `streams.c`, and the existing Android Media3 in `android/app/src/main/java/space/nuvio/nativelegacy/`. Android is already integrated into the release; initial research describing it as a separate branch predates the merge inventory.

The path with least external dependency is temporal AutoSync between subtitles and stream adequacy via diagnostics. Android has ready APIs for cache/gain and transfer instrumentation; access to PCM, gain, and app-controlled cache on LG/Samsung requires backend proofs. Do not promise the same capability across platforms prior to these proofs. Full initial research is in the local ignored file `docs/PESQUISA-AUTOSYNC-PLAYER.md` of the main checkout; this plan incorporates additional requirements and is trackable in version control.

## Preference and State Contracts

| Data | Scope | Proposed Policy |
|---|---|---|
| Primary/secondary languages, add-on choice | User profile | Synchronizable via account, respecting explicit choices |
| Performance, cache, allowed gain, speech model, and diagnostics | Device | Local; one account may have distinct settings on Android TV and Samsung |
| Seekr limit and daily usage | Device, all profiles | Never resets on account, profile, or key changes |
| Personal Seekr key | Device | Local, masked, removable; do not sync in plaintext |
| Automatic/manual offset | Session + document identity | Independent per language; source switch invalidates analysis |
| Throughput learning | Device + network + host/provider | Store origin, age, and quality; do not confuse slow CDN with slow connection |

Device identity: random UUID generated upon installation, no serial/MAC. An effective performance profile resolves `hardware default → device settings → explicitly local override`; account must not silently overwrite this outcome. Reinstalling or clearing data may erase the UUID and counter: strictly enforced per-device limits require authenticated backend device registration, not just local storage. Local MVP limits normal usage; this limitation must be documented.

## Subtitle AutoSync

Proposal: automatically enabled when a preferred external subtitle and reference are available, with an option to disable in Settings. Playback starts without waiting for the result. Apply only accepted corrections; maintain manual adjustment and Undo action. Do not switch the chosen track to produce a reference.

Refactor `LegendaDocumento` independent of overlay: cues, language, origin, identity/hash, and generation. The embedded reference resides in another document. The current ASS collector does not automatically cover SRT/MP4, and a forward sample does not prove the entire title. Cancellation uses session generation, source, and selection.

MVP: constant offset based on temporal activity, with text reinforcement only when compatible. Acceptance requires dialogue coverage, distinct peak against alternatives, and agreement across separate windows. Forced/signs, repeated intros, different cuts, and sparse dialogue must be able to produce rejection. Scaling and segment retiming are a later phase; discordant offset across regions must never become a global correction.

| Control | Proposed Semantics |
|---|---|
| Quick | Few references/windows and short budget; never lowers confidence threshold |
| Thorough | More references and regions; same threshold; cancellable background execution |
| Tolerance | Allowed residual error, separate from max search range |
| Try another reference | Excludes rejected reference in session and restarts search |
| Displayed state | Analyzing / Synchronized / No safe match / Unavailable |

Initial budgets for prototype, not measured result: Quick up to 5s processing and 3 regions; Thorough up to 20s and 6 regions, with separate ceiling for reference bytes. Do not apply during seek/menus if cost causes stuttering. Initial candidate tolerance 250ms with 150/250/500ms options: calibrate against human annotation error distribution before finalizing default.

## Secondary Subtitle and Simple Selector

Initial screen displays only configured primary and secondary languages. For each language, display embedded and add-on candidates with line `English · Embedded` or `English · OpenSubtitles`. Distinct selection indicator for primary/secondary without mixing languages. Embedded designates track origin, not external vendor.

More options action opens full list and details: track name, file/release, codec, forced/SDH, origin/add-on, format, delay, and AutoSync state. Hiding these details on initial screen must not prevent selecting another version. Selecting none for secondary must always be available. Preserve identity/focus on late arrival of add-ons.

Two documents, two offsets, and two selections. A reference may be reused only after individual validation. Define two overlay areas and avoid collisions with ASS controls/billboards. If backend does not support two native tracks, render secondary in app; test actual compatibility prior to enabling.

## Audio Sync

Separate toggle, initially off, clear copy on local processing. Candidate pipeline: mono PCM → VAD → temporal correlation; optional local ASR for English audio produces additional timed reference. Translated subtitles do not match English words directly: validate by timing and structure.

Download model once, advise Wi-Fi, display true size/hash, progress, cancel, and remove. No automatic download on movie startup. Silero VAD and sherpa-onnx are candidates; compare whisper.cpp for multilingual support without assuming lower latency. Do not infer total RAM from file size.

Passthrough/offload and encoded codecs might not deliver PCM. Do not silently disable passthrough, HDR/DV, or offload to analyze audio. If necessary, auxiliary decoding requires CPU/RAM and sample limits, or the feature becomes unavailable in that mode. External APIs only as separate evaluation with consent for clip upload; default pipeline does not transmit audio, signed URLs, or debrid headers.

## Seek Cache and Volume

Disk session cache: Off, 256/512/1024 MiB (product labels 256 MB/512 MB/1 GB). Reserve free space, reduce/reject configuration if insufficient space, asynchronous writes, cleanup on exit and on launch after crash. Start with progressive HTTP VOD; HLS/DASH/live do not inherit support without dedicated tests. Configured volume and prefetch budget are distinct controls. Seek will only be fast when target and decoding data are covered by cache.

Android: evaluate `CacheDataSource`/`SimpleCache` with `ParaleloDataSource`. HTTP Range, authentication, redirection, cancellation, and multiple connections must preserve contract. LG/uMS, Samsung WGT/AVPlay, and TPK: compatible proxy/local source proof before offering option. AVPlay internal buffer does not equate to app file cache.

Volume: 0–100% uses current path; 100–200% uses real gain where available. 200% amplitude corresponds to approximately +6.02 dB; `LoudnessEnhancer` takes gain in millibels, not percentage. Control clipping/limiting, indicate red above 100%, reset on session close/switch. Remote control bar accepts arrows without depending on swipe. Absent API or incompatible passthrough caps at 100%; do not raise TV physical volume to simulate player gain.

## Suitable Sources and Speed Test

Apply stable partition over chosen base order: suitable/unknown first, likely heavy later. Do not hide sources. Classification locked while selector is open to preserve focus; new responses enter with stable identity.

Demand: reliable bitrate or exact bytes × 8 / duration / 1e6. Season pack size does not serve episode; resolution alone does not prove bitrate. `vazao.h` already offers p20 × 0.75 as conservative budget and median × 0.9 as ceiling; reuse with provenance and expiration, not an arbitrary constant copied from fork.

Passive learning counts only media transfers with active fetching, excludes cache hits, pauses, and waiting for full buffer. Stall during active download is a signal; do not exclude it to inflate measurement. `ParaleloDataSource` already receives TransferListener: instrument bytes without duplicating connection counts. Closed LG/Samsung player requires own telemetry; in its absence inform that classification originates from diagnostics.

Speed test must offer: quick with known endpoint (network) and source/host test (actual path). Display sustained Mbps, margin, timestamp, and origin, alongside estimated demand when reliable. Do not guarantee resolution or absence of buffering. Invalidate on network change/retest and use candidate 24h expiration until calibration. Do not launch active tests during playback without user request; they compete with media. Count bytes and allow cancellation.

## Seekr — 50 Requests per Day per TV

User defined 50/day limit per device and return of personal key. In examined base e1f2936b, `seekr.h` maintains personal key API, while `ajustes.c` hides AJ_SEEKR_CHAVE/AJ_SEEKR_TESTAR when `seekrEmbutida()` is true (filter near line 3591). Restore visibility and review precedence of personal versus embedded key; editing operational API alone does not restore field.

Count each HTTP dispatch of `/sprites`, including retry/refresh; cached frames, VTT, and JPEG do not consume local quota. Durable atomic reservation BEFORE dispatch, mutex, and replacement write; crash after reservation may consume a call without sending it, accepted to avoid exceeding 50. Single-flight per title/duration/key identity avoids identical concurrent queries. Cache lookup respects validity; VTT/signed URL does not persist past expiration.

Proposed window: UTC day to align with vendor documentation; present reset time in local timezone. Save last observed day and prevent clock rollback from freeing counter; without reliable clock, use last valid window and degrade with explanatory message. For strong enforcement, backend must govern counter and window without exposing shared key in package. Using personal key does not raise 50 TV limit; vendor quotas remain independent and may be lower.

Distinct states: local limit reached, 429 vendor with Retry-After, 401/403 key rejected, title without preview, network unavailable. None of these blocks seek/playback. Masked field with test/remove; key validation is not a `/sprites` call and must not consume 50 quota, but respects its own limits. Do not log key, Bearer, signed URL, or response containing it.

## Diagnostics, Optimization, and Logs

Extend report: backend/build/device, free/peak memory, free storage, bitrate and bytes per media, startup/seek time, rebuffering, render/menu time, cache capacity/coverage, AutoSync time/result/error/rejection, VAD/ASR RAM/CPU, Seekr local usage/status/latency. Aggregate data; content and personal identifiers unnecessary.

The `perfiltv.h` table distinguishes LG, WGT, and TPK, but lacks explicit Android entry: add dedicated evaluation before sharing limits. Reuse decision with hysteresis and existing retest/rollback; automate only knobs whose improvement was measured with identical sample and respect manual override. Seekr limit, language, audio consent, and boost are not knobs to maximize via optimization.

New logs in English with stable events/reasons, e.g., `subtitle_sync rejected reason=ambiguous_peak`, `seekr_lookup blocked reason=device_daily_limit used=50`, `stream_fit classified reason=estimated_bitrate`. UI remains translated. Preserve compatibility with tools parsing old logs; module-by-module migration with consumer testing, without changing all texts at once.

## Feasibility Matrix

| Capability | Android/Media3 | LG/uMS | Samsung WGT/AVPlay | Samsung TPK |
|---|---|---|---|---|
| External documents/offset and simple selection | Common C path | Common C path | Common C path | Common C path |
| Embedded reference | Extraction/track to demonstrate | ASS Range exists; expand | ASS Range exists; expand | ASS Range exists; expand |
| Actual playback bytes | Instrumentable DataSource | Closed API: pending proof | Closed API: pending proof | Multimedia contract: pending proof |
| PCM for VAD/ASR | AudioProcessor/decoder; exclude incompatible modes | Pending proof | Pending proof | Pending proof |
| App file cache | Media3 candidate | Source/proxy: pending proof | Source/proxy: pending proof | Source/proxy: pending proof |
| Boost above 100 | LoudnessEnhancer candidate | Gain/PCM: pending proof | Gain/PCM: pending proof | Gain/PCM: pending proof |
| Seekr quota / local performance | Common with storage adapter | Common | Common | Common |

## Ready Dependencies and Official Sources

| Project / API | Reuse | License / Condition |
|---|---|---|
| [alass](https://github.com/kaegi/alass) | Temporal engine / baseline; Rust-C FFI needed | GPL-3.0; verify compatibility and toolchains before vendoring |
| [ffsubsync](https://github.com/smacke/ffsubsync) | Baseline for reference and audio | MIT; Python/FFmpeg inadequate for embedding without footprint study |
| [Silero VAD](https://github.com/snakers4/silero-vad) | Local speech detection | MIT; weights/runtime require inventory |
| [sherpa-onnx](https://k2-fsa.github.io/sherpa/onnx/index.html) | ASR with native interface | Apache-2.0 in project; verify license of each model |
| [whisper.cpp](https://github.com/ggml-org/whisper.cpp) | Alternative C/C++ ASR | MIT in project; verify models and target RAM |
| [Media3 network stacks](https://developer.android.com/media/media3/exoplayer/network-stacks) | Android cache / transfers | Apache-2.0 in AndroidX libraries; use compatible pinned version |
| [LoudnessEnhancer](https://developer.android.com/reference/android/media/audiofx/LoudnessEnhancer) | Gain per session Android | Platform API; availability depends on device |
| [Seekr docs](https://seekr.tv/docs) | HTTP `/sprites`, optional Android 0.2.0 SDK | Service with quotas/terms; SDK requires license review before adoption |
| [TypeSafe](https://docs.typesafe.ai/introduction) | Optional semantic judgment | Proprietary API; typed output does not prove timing |
| [AVPlay](https://developer.samsung.com/smarttv/develop/api-references/samsung-product-api-references/avplay-api.html) | Capabilities / backend buffer | Platform API; internal buffer is not disk cache |

Jev may prioritize references by clip/metadata, but does not calculate offset or replace temporal validation. Start in observation mode and compare against local rules. Without measured gain, remove from critical path. Credential stays on backend; send minimal clips without private URL/headers. ElevenLabs forced alignment is an external alternative when text matches speech, not a direct solution for translation. External APIs were not invoked in this task.

## Sequence and Release Criteria

1. Separate device/account preferences, Seekr quota, and diagnostics; migration preserves current behavior.
2. Simple selector, independent documents, and stable source partition with diagnostics.
3. AutoSync offset single language on host, then extraction on each backend; secondary language subsequently.
4. Android cache/boost; VAD and ASR opt-in only after PCM proof.
5. LG/Samsung proofs; capability enablement rather than platform macro alone. Jev only following comparative evaluation.

Benchmark on LG C9, Android TV/TCL identified by model/SoC/build, Samsung identified and WGT/TPK separated. Use identical legal test files, hashes, controlled network, pinned builds, cold/warm runs, at least 10 repetitions per scenario; report median/p95 and failure counts. Do not aggregate platforms to hide regression.

Temporal corpus: offsets ±0.25/1/5/30s; incompatible edit / cut in middle; scale 23.976→25; music, sparse speech, repetitive dialogue, forced/SDH; embedded SRT/VTT/ASS; translated subtitles and two languages with different offsets. Measure error against human annotation, correct accepts, false corrections, and correct rejections. Proposed initial gate: zero false applications on adversarial set, p95 error ≤ configured tolerance among accepts; state corpus size explicitly, as zero observed does not guarantee zero in production.

Performance: startup time to first frame, seek inside/outside cache and keyframe, rebuffer per hour, frames/menu p95, peak RAM, CPU, watts if available, and additional bytes. Compare 1.8 baseline without feature versus each isolated feature and combined. Every measurement must include codec/HDR/passthrough/storage/network. Choose method with lowest latency AMONG those passing effectiveness and not degrading playback; do not reward fast and incorrect corrections.

Required deterministic tests: generation/cancellation; independent and manual offset; 50 quota with concurrency/restart/clock rollback/profile/key; cache ENOSPC/cancellation/crash/Range; stable partition with missing metadata and incremental arrival; isolation of same account on two TVs; optimizer rollback and overrides. Physical proofs: video remains responsive during analysis, seek genuinely improves under coverage, audio does not clip/reroute, ASS/secondary language overlays do not collide.

## Personal Media Servers

Jellyfin, Emby, and Plex must share capabilities negotiation, bitrate, languages, and progress with this player: [research and sequence](../media-servers-1.8/README.md). Servers are a separate future scope; not yet implemented.
