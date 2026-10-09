# Subtitle Sync via Audio (F06): Backend Capabilities, Implementation, and Optional Model

10/04/2026, branch `f06-audio-sync` (base `50afbae3`). This document separates what was **proven in code** from what **requires physical TV testing**. Nothing here has been measured on physical devices.

## 1. Capability Matrix: Can the app read decoded PCM from playing audio?

| Backend | Capability | Device | Evidence | Blocked by |
|---|---|---|---|---|
| **Android (Media3 1.8.0 / ExoPlayer)** | **Enabled in code** (compilation proof + JVM replay; no TV proof) | Android TV (TCL C755 cited in F03; not tested) | `AudioSyncSink.kt`: `ForwardingAudioSink` over `DefaultAudioSink` of `DefaultRenderersFactory.buildAudioSink`. `handleBuffer(buffer, presentationTimeUs, …)` delivers input PCM from the sink **with media time** for each buffer. `configure(Format)` indicates if input is `audio/raw` (PCM) or bitstream. Compiles against android-35 + Media3 1.8.0 (`tests/audiosync_android.py`). | Nothing structural. Bitstream (passthrough AC3/E-AC3/DTS/TrueHD, or offload) **does not pass as PCM** in app: reported as "unavailable with passthrough enabled", never disabled. |
| Android: `TeeAudioProcessor` alternative | Discarded | — | `javap` of Media3 1.8.0: `AudioBufferSink.flush(int,int,int)`/`handleBuffer(ByteBuffer)` without timestamp; `AudioProcessor.StreamMetadata` does not exist in 1.8.0. Without media time, alignment would depend on counting samples since flush. | — |
| **LG webOS (uMS / starfish, `video.c`)** | **Unavailable** | LG C9 (not tested) | The app delivers URI to uMediaServer via Luna; the pipeline decodes and plays in hardware. No uMS/ACB API returns PCM to the app process. NDL is an *output* path (app pushes audio/video), not capture. | Closed platform. Feasible only with **auxiliary decoding**: download and decode audio track in parallel in app (Range + custom decoder), with CPU/network budget and proof that it does not degrade playback. Not implemented. |
| **Samsung .wgt (AVPlay)** | **Unavailable** | — | AVPlay API has no audio data callback; the AVPlay object is not an `HTMLMediaElement`, so Web Audio (`createMediaElementSource`) does not apply. | Closed platform; same auxiliary decoding alternative (in WASM, without raw sockets, even more expensive). |
| **Samsung .tpk Tizen 6+ (Tizen.Multimedia.Player, `video_tpk.c` + `Video.cs`)** | **Unavailable in product; candidate pending proof** | — | TizenFX has `Player.EnableExportingAudioData(AudioMediaFormat, PlayerAudioExtractOption)` + `AudioDataDecoded` event (since Tizen 6.0). Documented constraints: call in `Idle`, unavailable with audio offload and for some codecs. Documentation is ambiguous on whether exported audio **continues playing** (`Default` mode "synchronizes with clock"), and whether the TV player (hardware pipeline) supports it. | Requires physical TV experiment: does audio remain audible? A/V sync and HDR intact? CPU cost? Until then, `audsync_backend` is not registered in .tpk and UI displays "unavailable on this platform". |
| Samsung .tpk Tizen 4/5 (`NuvioTpk40`) | **Unavailable** | — | Above API requires Tizen 6.0+. | Platform version. |

Rule: capability is announced **by the backend at runtime** (`audsync_backend`, registered on first `nativeAudioEstado` call from Kotlin shell), not by platform macro. An older Android shell lacking the tap remains "unavailable on this platform".

## 2. What this slice delivers (Android, end-to-end in code)

1. **Setting** `Languages and subtitles › Audio synchronization` (`AJ_LEG_SYNC_AUDIO`, last in `OpcaoId`, key `legendaSyncAudioLocal`, per-TV local, default **off**, Lucide icon `audio-lines`). Off = nothing audio-related appears.
2. **Tap** (`AudioSyncTap.kt`, pure Kotlin): only copies when `enabled` (C enables via `escolher(3,1)`); downmix of all channels + box filter downsampling to **mono 16 kHz int16**; PCM 8/16/24/32/float; reads `ByteBuffer` by absolute index (player position/limit untouched) and writes to fixed `ShortArray(2048)` — **zero allocations per buffer**. Same `(buffer, pts)` re-offered by sink is copied once. `flush()` = discontinuity.
3. **JNI** (`video_android.c`): `nativeAudioPcm` copies to stack and passes to `audsync_pcm`; `nativeAudioEstado` reports PCM/bitstream/none.
4. **Session** (`src/audsync.c`): bounded ring of 4s (`AUDSYNC_RING`) + 512 descriptors under short lock; producer never waits — full ring discards and breaks window. Worker thread runs VAD up to **300s contiguous** media (engine requires ≥180s overlap after offset); seek/pts jump > 200ms, format change, or pause due to seek/buffering resets window. Bitstream during listening → "unavailable with passthrough enabled".
5. **VAD** (`src/audvad.c`): 20ms frames; 300–3400 Hz band energy above adaptive floor (minimum 5s) + band fraction + energy-weighted spectral flux (512 FFT). Onset 3 frames, release 15, segment ends at last active frame; merges gaps < 300ms, discards < 200ms.
6. **Candidate alignment**: speech activity × cue activity (dialogue only) on 50ms grid, chance-corrected Jaccard, ±30s. Pre-rejections: no speech, continuous audio, no subtitle activity, confidence < 0.30, margin < 0.08, short window.
7. **Engine decision**: with candidate offset, chooses silence cut points on both timelines and builds two `LegendaDocumento` instances for the **same window**: speech (distinct labels `speech NNNN`, origin `audio`) and trimmed external subtitle. `legsync.c` selects trim and invokes `autosync_solicitar` with `radius` 30s and **standard thresholds** (0.78/0.09, regions, boundaries ±250ms). Only ACCEPTED alters offset; Undo preserves manual. "Another reference" does not apply to audio.
8. **Subtitle selector** (F04/F05 AutoSync row): "By audio" action between ‹ › when available; "Listening to speech… N%" with Stop; accept/reject as in F05; "By audio: no clear speech; nothing modified"; at rest, with setting on and without PCM, displays reason ("unavailable on this platform" / "with passthrough enabled" / "no decoded audio").
9. **Cancellation**: closing player, source change, subtitle change, audio track switch (`faixas.c` → `legsync_audio_trocou`), turning off setting, Stop. The tap is disarmed on UI thread (`audsync_passo`).

## 3. Verification

```sh
bash tests/audsync.sh                 # 23 cases; SANITIZE=1 and SANITIZE=thread: zero errors
python3 tests/audiosync_android.py    # compile-check NvPlayer/AudioSyncSink/AudioSyncTap + JVM tap replay
python3 tests/streamfit_passiva_android.py   # continues passing with new files
bash tests/legsync.sh; bash tests/autosync.sh; bash tests/legref.sh   # + SANITIZE=1/thread
bash tests/ajustes_ux_dados.sh; bash tests/ajustes_secoes.sh; python3 tools/idiomas.py
bash tests/audsync_shot.sh /Volumes/ExternalSSD/nv-f06-shots/audsync   # GL screenshots, Montserrat
```

Synthetic cases: VAD with speech bursts in noise (boundaries ≤100ms), noise/music/silence without speech; recovered offsets +2.5/−1.2/+12/−25/0s ≤50ms; music-only window rejected; speech from different "movie" rejected; complete session accepts +2.5s and −1.2s (with intermediate seek resetting window), music rejected with zero changes; platform without PCM, setting off, passthrough before and during listening; cancellations above; ring with non-blocking producer (exactly what fits is accepted, remainder counted as dropped and window resets). NDK 27.2 `-fsyntax-only` arm64/ARMv7 of `video_android.c`/`audsync.c`/`audvad.c`/`legsync.c`. Full host build of core.

## 4. Honest Limitations

- **No physical TV proof.** Pending: whether tap actually receives PCM on TV DefaultAudioSink; ARM CPU cost of VAD; real latency; behavior with extension FFmpeg decoder.
- **Speech under loud music is not detected** (floor rises along) → rejection. Subtitles with long reading times / cues merging multiple lines will trigger engine boundary ±250ms rule and be **rejected**; this is intentionally conservative. Improving recall requires annotated real corpus before relaxing any threshold.
- 300s listening window: correction arrives after ~5 min of playback.
- Primary slot only; no secondary language.
- New logs in English: `[audsync] audio_sync window reason=…`, `[legsync] audio_sync rejected reason=…`; no text, language, URL, or media identity.

## 5. Optional Local ASR Model — DESIGN ONLY (Not implemented)

Objective: when VAD alone is insufficient (speech under loud music, cues markedly differing from speech), transcribe English segments and align **text** with subtitles (simple forced alignment), returning same document pair to engine.

- **Candidates**: whisper.cpp `tiny.en`/`base.en` (MIT; ~75/142 MB ggml, q5 ~31/57 MB) or sherpa-onnx with small Zipformer (Apache-2.0; ~70 MB int8). Silero VAD (MIT, ~2 MB ONNX) could replace heuristic VAD. Verify license of **each weight file** before offering.
- **Explicit one-time download**: action in Settings "Download speech model (English) — N MB", showing size beforehand; download to temporary file with Range/resume, **fixed SHA-256 in app** (hash mismatch = delete and reject), progress, **Cancel** and **Remove** (frees space). Never automatic, never during playback; checks free space (`statvfs`) with 2× buffer before starting. Per-TV local.
- **Budget**: peak RAM ≤ 150 MB (tiny/base q5 fit; measure on 1.5–2 GB ARM TV); CPU on 1 low-priority thread, only 3–6 speech-rich 20s segments (selected by VAD), target < 30s total processing for Quick; aborts if player drops frames/buffers (same pause mechanism as F05).
- **English first**: only offered when audio track and subtitle are `en`; other languages only with multilingual model and new evaluation.
- **Gate**: benchmark VAD vs ASR for latency, correct accepts, false corrections, and RAM on identical corpus before enabling by default; without measured improvement, do not include.
