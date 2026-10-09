# Temporal AutoSync — Common Engine and Subtitle Documents

10/03/2026. C implementation in `src/autosync.c/h` and adaptation of `src/legenda.c/h`, following Glass merge `b98bd2f5`. This document describes code capabilities and host tests. It does not prove embedded reference availability nor performance on LG, Android, or Samsung.

## 2.1 (10/06/2026): Parity with Nuvio Reshaped AutoSync

The engine was rewritten (ideas from PR NuvioMedia/NuvioTV#3703, GPL-3.0 like this project; original implementation). Where this document mentions "constant offset only", "boundary ±250ms rule in both directions", or "MP4 unavailable", the following takes precedence:

- Scale (frames per second): exact ratios 25/23.976, 24/23.976, 25/24, and inverses + span ratios, refined via least squares. Offset up to ±180s; entire reference when external covers < 75% of it (CD1/CD2).
- Banded dynamic programming (±28 dialogue cues) with 1:1, 1:2, 2:1, 1:3, 3:1, 2:2 clusters; segments with custom offsets for extra cuts/scenes (Viterbi); line anchors only where all 7 neighbors agree. Scale/segments are applied via `AutoSyncMapa` (video timestamp → subtitle timestamp: `legsync_posicao`, libass, and app rendering), without reloading the stream.
- Gates: activity ≥ 0.55, final score ≥ 0.78 on corrected subtitle, margin ≥ 0.02 (per segment on cut), coverage ≥ 0.84 (0.90 with scale/segments), ≥ 20 matched cues, per-segment coherence (median absolute error ≤ tolerance). Rejection preserves existing state; corrections smaller than `manterMs` (100ms) retain original.
- Reference: MP4/MOV via `moov` (tx3g/wvtt/text); MKV SRT with CueDuration extracted directly from Cues (3 Ranges in feature film, previously ~1300); tracks ordered and cross-validated (preferred is dropped if offset conflicts with two concurring tracks).
- Secondary language (slot 1) using same engine and reference.
- Measurement: `ANTES=1 bash tests/autosync_corpus.sh` (synthetic corpus, before vs. after comparison table).

## What Functions in this Module

- Immutable documents independent of overlay, with language, origin, opaque track/file identity, session generation, flags, and ordered cues. Reference counting allows sharing a validated reference between two slots without copying document.
- Native parsing of SRT, WebVTT, and ASS/SSA; existing UTF-8, UTF-16, and Windows-1251/1252 conversion. Document builder preserves valid body for rendering, but clears `COMPLETO` when parser dropped invalid timestamp or obtained only a prefix due to budget/allocation limits. Legacy parser behavior for playback remains identical.
- Temporal activity correlation across merged intervals, without requiring identical text across languages. Text only detects excessive repetition; it provides no semantic guarantee that files belong to identical movie.
- Two selection slots and two independent automatic/manual offsets. Undo resets only automatic offset. Switching document/session cancels analysis and invalidates both offsets for that selection.
- Single worker per player context, with cancellation via session generation and selection, time ceiling, alternating queue between slots, and results published only if owners remain active. UI queries state and offset; no background thread callbacks alter playback.
- Try another reference excludes latest identity/hash for that session and selection, cancels stale results, and reverts its automatic correction. Up to 16 exclusions; exhausting list makes search unavailable for that selection, without forgetting rejected references.
- New events in English, with stable reason, slot, offset, score, and duration. No cues, language, media identity, URL, headers, or credentials logged.

## Acceptance and Cost

Candidate offset originates from start difference histograms, quantized to 25ms. Top 24 voted peaks receive activity Jaccard score corrected for expected coincidence. No audio transmission, downloads, track changes, network requests, or external dependencies in this path.

Same threshold applies in Quick and Thorough: minimum global score 0.78 and minimum margin 0.09 over alternative hypotheses; per region, minimum score 0.72 and minimum margin 0.07. `confianca` is a heuristic score, **not a calibrated probability of correctness**. Adjusting tolerance does not alter thresholds, search radius, or fixed spacing separating hypotheses.

Quick validates 3 regions, Thorough validates 6, distributed across total coverage. In addition to average, each dialogue interval must have matching boundaries in both documents within tolerance, in both directions. Consequently, a short cut in middle/end cannot hide behind many correct cues. `erroMs` reports highest observed residual on boundaries or discordance across regional offsets. Does not perform scaling, interpolation, or segment retiming.

Rejected items: partial documents; forced/signs flags; excess music/SDH cues or top-positioned ASS; heavily repetitive speech; under 30 useful cues / 24 intervals; under 180s coverage or 40s speech; sparse or near-continuous activity; ambiguous peak; region/boundary disagreement; stale generation; exhausted budget. If reliable media durations were supplied for both, difference exceeding tolerance also rejects pair.

Initial code values, subject to human corpus calibration: tolerance 250ms, independent radius ±60s, Quick budget 4s, Thorough budget 16s. API accepts tolerance 50–1000ms, radius 1–120s, and budget 1–20000ms. Cancellation/budget is checked between batches; not a guarantee of preemption on exact millisecond. Maximum 7999 cues accepted for synchronization; 8000 suggests parser truncation and triggers rejection. Document bodies capped at 16 MiB. Each document preserves full `LegendaCue` for rendering; footprint depends on cue count and can reach ~6.6 MiB per document. Analysis uses compact interval/hash vectors and histogram without copying text per hypothesis.

Boundary rule is deliberately conservative. Translation with differing segmentation/timing may be rejected even if from identical edit. Continuous cuts and overlapping cues merge, but differing gaps and timing outside tolerance trigger rejection. Improving recall requires human corpus prior to relaxing global acceptance.

## Integration Contract

```c
AutoSync *sync = autosync_criar();
autosync_iniciar(sync, sessaoDoPlayer); /* monotonic per-media generation */

LegendaDocumentoInfo info = {
  .sessao = sessaoDoPlayer,
  .flags = LEGENDA_DOC_COMPLETO,
  .idioma = "en",
  .origem = "OpenSubtitles",
  .identidade = "opaque-addon-subtitle-id"
};
LegendaDocumento *doc = legenda_documento_bytes(bytes, tamanho, &info);
autosync_selecionar(sync, 0, doc); /* 1 = secondary language */
legenda_documento_liberar(doc); /* context retained its own reference */

/* Only provide a REAL, complete reference from the same session. */
AutoSyncConfig cfg = autosync_config(AUTOSYNC_QUICK);
autosync_solicitar(sync, 0, referenciaCompleta, &cfg);

/* On UI/render thread: */
AutoSyncResultado estado = autosync_estado(sync, 0);
int offsetTotal = autosync_offset_ms(sync, 0);
/* Positive advances in current overlay: t = playback + offsetTotal/1000. */
```

Key points in player/tracks:

1. Create context at a point that does not delay first frame; start new generation when opening/switching media and clear selection when disabling/switching subtitles. Do not wait for analysis before `video_iniciar`/play.
2. Build documents on worker thread that already downloaded bytes. For existing overlay snapshot, `legenda_documento_ativo(legenda_geracao(), &info)` copies only if generation still owns it; sort/hash run outside overlay lock. Snapshot does not determine language/origin/completeness: caller must provide accurate metadata.
3. Import current manual adjustment with `autosync_manual`. Render total offset exactly once. Sign matches existing `legenda_cues` and `assrender_desenhar`, where positive advances; do not assume native backend shares same sign. Do not add `legEstilo.atrasoMs` again when `offsetTotal` already includes it.
4. Primary slot: pass total to SRT/VTT overlay or `assrender_desenhar`. Secondary slot: `legenda_documento_cues` allows querying cues with distinct offset in another render area. Two documents/offsets are implemented; **secondary libass overlay / collision management still requires player integration**, and current libass renderer is singleton.
5. If in sensitive seek/menu or experiencing CPU/network contention, use `autosync_cancelar`; when safe, request again with real reference. API does not contest media connection.
6. Map `UNAVAILABLE/ANALYSING/ACCEPTED/REJECTED/CANCELLED` state to translated messages. `autosync_tentar_outra` only excludes; fetching next reference is caller responsibility, respecting capabilities and byte budget.
7. `autosync_desfazer` preserves manual. `autosync_iniciar` clears selections, offsets, and exclusions. `autosync_destruir` cancels and joins; use in teardown, not playback startup. Do not invoke functions following destruction.

### Embedded Reference: Real Baseline Constraints

`mkvass.c` currently collects selected ASS/SSA track and publishes to overlay for that selection. It does not provide an independent secondary document for the selected track. Partial forward window does not represent remainder of movie. `mkvass_iniciar` switches collection and therefore **must not be called to synthesize an AutoSync reference**. Embedded MP4/SRT and parallel full extraction are unavailable in current API.

A complete ASS reference from existing sidecar/collector may become `LegendaDocumento` only after caller confirms origin, track, session, and completeness. A full external reference may be used by engine in explicit testing/comparison, but is not equivalent to an embedded reference. In absence of genuine complete reference, product must retain zero automatic offset and show unavailable, without delaying or altering user selection.

VAD, PCM, speech model, AudioSync, model downloads, and ASR APIs were not implemented by this module. (F06 later introduced audio reference on Android without modifying engine thresholds: see `AUDIOSYNC-CAPABILITY.md`.) No new models or licenses were added. Separating these capabilities avoids claiming audio sync merely because temporal engine exists.

## Executed Verification

```sh
bash tests/autosync.sh
SANITIZE=1 bash tests/autosync.sh
SANITIZE=thread bash tests/autosync.sh
bash tests/legenda_ass.sh
cc -Isrc -Wall -Wextra -Werror -fsyntax-only src/autosync.c
```

562 deterministic checks on host: offsets ±0.25/1/5/30s in both modes; translation with compatible activity; real SRT/VTT/ASS; character sets; immutable snapshots following overlay switch/disable; forced/signs/music/repetition; periodic temporal ambiguity; sparse dialogue; differing edits, 23.976→25 scaling and drift; single cue cut in middle/end; ±100ms noise accepted at 250 tolerance and rejected at 50; independent radius; budget; dual languages / manual / undo; exclusion; stale generation/selection; 100 distinct timelines per mode with adversarial cuts; 7999-cue documents and 100 session switches while busy.

ASan/UBSan and ThreadSanitizer pass without observed issues. Legacy ASS/SRT/VTT parser tests continue passing. Timelines are synthetic and offsets have deterministic ground truth; an annotated real legal subtitle corpus, false accept/recall benchmarks, and physical hardware benchmarks remain needed. Maximum observed runtime in normal synthetic run on Mac was 24ms, and 102ms under ThreadSanitizer; this does not predict TV latency.

Full compile, TV deployment, publishing, and backend/player integration were not conducted by this isolated module.

## Player Integration (F05, 10/04/2026)

Three new modules link engine to player, single language (primary slot):

- `src/legref.c/h` — **independent** embedded reference collector. Reads Matroska text track (S_TEXT/UTF8, ASS, SSA) via HTTP Range on dedicated thread using only index (SeekHead → Cues → CueRelativePosition). Does not invoke `mkvass_*`, `legenda_carregar`, `assrender_*`, or `video_*` (verified via `nm` in `tests/legref.sh`): user track does not change. Document is marked `COMPLETO` only when all indexed blocks arrive; missing track Cues, missing BlockDuration, lacing, non-Range server (200), or exhausted budget → unavailable with reason. Signs/forced never serve as reference. Prefers language of external subtitle; TrackNumber exclusions for "another reference". MP4/tx3g not read. Disabled on WGT/AVPlay and `.tpk` (`legref_disponivel`), where UI reports "Unavailable on this platform".
- `src/legsync.c/h` — session: new generation in `player_abrir`, cancellation in `lembrarFonte` (player close), URL switch detected in `player_atualizar` (source switch), track change in `faixas.c` (embedded/none/other external). External becomes `LegendaDocumento` directly on download thread (`legenda_carregar_com`). Reference read only upon user request (Quick/Thorough); seek, buffering, and video buffer < 20s pause reading and cancel analysis, resuming 2s after calm. Budget: 12 MiB per read, 24 MiB per media, 8 Ranges/s. `legsync_offset_ms(manual)` returns manual + accepted auto, applied once in `desenharLegendaExterna`, and only while analyzed document owns overlay.
- `src/legsyncui.c` — provider (`legsync_ui_ligar` → `legendasui_definir_sync`) for "Automatic synchronization" row in F04 subtitle selector (`legendasui.c`, expanding from clock island). Row exists only for primary slot, active external subtitle, and non-live channels: translated status below title, action between ‹ › (Quick, Thorough, Undo, Another reference, Stop), "Unavailable on this platform" when collector absent. Excluded from "N of M"; when focused, footer explains OK action. Subtitle selection routes via `faixas_escolher_externa`/`escolherLegenda`, calling `legsync_primaria_*`.

Never delays playback start: context creation does not touch network; nothing read prior to request. Only ACCEPTED result modifies offset; rejection shows "Insufficient confidence; nothing modified".

### Verification

```sh
bash tests/legref.sh     # 17 cases + real HTTP (rede.c/libcurl): complete, redirect, non-Range
bash tests/legsync.sh    # 14 cases (includes selector provider): accept +2.5s/−1.2s, undo, another ref, seek,
                         # source/track change, delayed download, cancellation with stuck Range
                         # (close, source, embedded, other external, disable), teardown, platform
SANITIZE=1 / SANITIZE=thread on both and on tests/autosync.sh: zero errors
bash tests/legsync_shot.sh DIR   # GL screenshots, Montserrat, real HTTP
```

Real fixtures via ffmpeg and mkvmerge (`tests/legref_fixtures.sh`).

### Limitations

- Fixed 250ms tolerance (engine default); no setting in UI.
- Secondary language: engine supports two slots, player currently uses only primary; slot 1 returns "available later".
- Reference only from MKV with Cues per subtitle block (written by ffmpeg and mkvmerge). MP4, MKV without track Cues, and bitmap subtitles (PGS/VobSub) remain unavailable.
- No physical TV proof: real concurrency with video stream on LG/Android, Range-limiting CDNs, and read times on long films. No human corpus yet.
