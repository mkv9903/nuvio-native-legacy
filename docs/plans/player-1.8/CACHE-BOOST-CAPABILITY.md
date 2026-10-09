# F07 — Disk Seek Cache and Volume up to 200%: Backend Capabilities

10/04/2026, branch `f07-cache-boost`. Complements the "Seek Cache and Volume" section of [README](README.md) and line F07 of [PLAN](../../releases/1.8.0/PLANO.md).

## Matrix

| Backend | Disk Seek Cache | Volume above 100% | What UI Displays |
|---|---|---|---|
| Android (Media3) | **Implemented**: `SimpleCache` + `CacheDataSource` per session | **Implemented**: gain `AudioProcessor` in `DefaultAudioSink` | 256/512/1024 MB option; volume slider 0–200% |
| LG webOS (uMS) | Unavailable: app only passes URL to pipeline | Unavailable: app does not see PCM; uMS `setVolume` caps at 100 | "Not available on this TV" on both |
| Samsung .wgt (AVPlay) | Unavailable (see below) | Unavailable | "Not available on this TV" on both |
| Samsung .tpk (native AVPlay) | Unavailable | Unavailable | "Not available on this TV" on both |

## Android: What Was Implemented

- **Cache** (`CacheMidia.kt`, `CacheSessao.kt`): `CacheDataSource` resides ABOVE existing chain (`DefaultDataSource` → cache → `ParaleloDataSource.Factory` with auth headers, Range, redirects, and 4 connections). Gaps route upstream with identical `DataSpec` (position/size = Range); already downloaded segments return from disk. Cache hits do not pass through `ParaleloDataSource`, ensuring StreamFit passive meter continues counting strictly socket bytes.
- Progressive HTTP VOD only. HLS/DASH (`.m3u8`/`.mpd` or anything `ParaleloDataSource.serve` rejects) remain excluded (`NAO_SE_APLICA`); live channels never activate cache (`player.c` sends 0). Trailers do not activate: play without cache at 100%.
- One folder per session under `<cacheDir>/nv-seek/<boot>/<session>`, no database (in-folder index), LRU at effective limit. Closing player or switching source frees and deletes directory off main thread (1.5s delay to allow cancelled loaders to complete). On startup, any folder from a different `boot` (dead process) is purged; symlinks are never followed.
- `statvfs` (`File.usableSpace`) before opening: largest size ≤ requested that leaves 512 MB free; unknown space = disabled. If nothing fits → `POUCO_ESPACO` and short banner in clock island.
- ENOSPC / intermediate write error: protected sink (`CacheSessao.Escrita`, with `CacheDataSink` without internal buffer) shuts down session cache, commits only confirmed prefix, and video playback continues (`DISCO_CHEIO` + single warning).
- Diagnostics: "Seek cache" with used/limit (or reason) in Large panel of performance overlay, updated every 2s.
- **Volume** (`GanhoAudioProcessor.kt`, `GanhoMath.kt`): 0–100% maps to `ExoPlayer.volume`; 100–200% applies real gain to PCM (200% = +6.02 dB), with smooth limiter (tanh above −1 dBFS), linear below it, untouched at 100%. Passthrough/bitstream does not pass through processors: slider remains capped at 100% with explanation; passthrough is never silently disabled. Chosen over `LoudnessEnhancer` because it applies to all PCM output including FFmpeg decoder, does not depend on TV session effects, and math is unit-testable.
- Coexistence with F06 (audio sync): sink is `AudioSyncSink(DefaultAudioSink[GanhoAudioProcessor])`. F06 listener reads `handleBuffer` at sink INPUT (decoded PCM with media time, prior to processing), so sync analyzes signal without boost or limiter; F07 gain operates in processor chain heading to output. PCM vs bitstream detection is unified: input format reported by `AudioSyncSink.configure` via `nativeAudioEstado` feeds both `audsync` and boost state (`cacheboost`). PCM offload is not enabled in this player; if introduced, boost logic must account for it.
- Volume is session-scoped: resets to 100% on each opened title; source switch and reconnect reapply session value.

## LG and Samsung: Why Unavailable

- **AVPlay internal buffer is not disk cache.** `setBufferingParam` controls how much memory player buffers before/after pauses; no API exists for app to persist bytes to file nor for player to read partial file during download. Does not prove seek cache.
- **uMS (webOS)** receives URL and downloads independently; app does not see media bytes or PCM. Offering cache would require local proxy (app serving `http://127.0.0.1` with Range from custom file), which is unproven across TVs (local socket permission in .wgt is absent; LG/TPK would require throughput and Range proofs under pipeline).
- **Gain**: without access to decoded PCM, only alternative would be elevating physical TV volume, prohibited by plan ("do not raise TV physical volume to simulate player gain").
- Consequently, both options display "Not available on this TV" on these backends (compile-time flag: `NV_ANDROID`), rather than presenting a non-functional control.

## TV Testing Requirements (Not proven in this slice)

- APK build and physical playback: seek within cached range measurably faster than uncached, disk/CPU consumption, behavior with 4K remux in TCL 192 MB heap (cache is disk; RAM buffer ceiling unchanged).
- Physical ENOSPC on full storage; cleanup upon process termination.
- Volume above 100% audible and free of clipping on actual PCM audio, and passthrough status with AC3/E-AC3 receiver (reported via sink input format via F06).
