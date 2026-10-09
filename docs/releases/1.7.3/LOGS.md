# Logs Reviewed for 1.7.3

Remote read-only D1 query, SELECT of 300 recent uploads identified as 1.7.1 or 1.7.2. Query response confirmation: rows_written=0, changes=0, changed_db=false. Raw bodies private outside Git; report excludes personal identities, tokens, source URLs, or private endpoints.

The slice contains 76 uploads identified as 1.7.2: 20 webOS, 17 tizen-tpk, 39 Android. Received between 2026-10-03 01:53:36 and 04:53:20 UTC; query IDs 19441–19958. Part precedes stable publication at 03:26:27 UTC. Version label does not prove published package or installed commit. Bodies are distinct, but several duplicate segments of identical playback sessions: do not count uploads as independent sessions or incidents.

## Findings

- Zero explicit FATAL EXCEPTION, Fatal signal, Segmentation fault, RuntimeError, or Aborted patterns in 1.7.2 slice. Does not prove absence of crashes or general stability.
- Origin/addon failures (408, 4xx, 5xx), Trakt 401, and missing data/artwork 404. HTTP 404 on intro/Seekr may signify content unavailable; do not treat automatically as client defect. URLs not exposed in this document.
- Samsung records player errors 0xfe6c0031, 0xffffffff, 0xfe6c0026, and 0xfe6c002e. In context of one upload, playback fails and Guide attempts next source, but diagnostic reports empty error. The candidate preserves the error code without assigning unproven causes.
- Home artwork delays and low framerates in some segments. Extreme timings may span player/background suspension; cache eviction counts are not OOM. Existing cache includes priority, screen protection, and per-device budgets; do not alter limits automatically based solely on this sample.
- Seekr responded both 200 with cues and 404 without preview. Per-TV quotas, personal keys, and fallback policies remain in 1.8 scope.

## Required Validation

Reproduce Samsung errors with confirmed source/codec and host identity; verify Trakt expiration with consented session; benchmark artwork and framerate latency on hardware under identical load. Data insufficient to assign all failures to Nuvio or promise performance gains in this candidate.
