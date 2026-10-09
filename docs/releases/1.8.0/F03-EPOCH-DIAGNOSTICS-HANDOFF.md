# F03 — Epoch and Diagnostics: Stable Milestone, Execution Paused

2026-10-03. Checkout: `codex/integration-180-glass`, at
`/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy`.
This document complements `HANDOFF-NEXT-AGENT.md`. The user requested finishing
what was already running and handing off continuity. **Runtime/duration wiring was
not started. Zero commit, staging, installation, or release occurred.**

## Concrete Delivery

- Android monitors default network via `ConnectivityManager`: route changes,
  relevant capabilities, link properties, blocking, and disconnections
  immediately invalidate history. Awaits Internet+Validated capabilities and
  link properties before announcing known network. Sequence is opaque, monotonic,
  and process-scoped; zero storage or logging of SSID/IP/credentials.
  Stale callbacks from alternate networks or post-destroy are ignored. Monitor
  registration failure marks network unknown. Monitor unregisters in destroy.
  JNI publishes invalidation and epoch under identical C lock.
- `redemarca.c/h` starts at zero, rejects stale notifications, and clears engine
  upon epoch change, including disconnect/reconnect. Zero constant epochs,
  LAN fingerprints, or health-check sequences created.
- N01 received disposable GET: stores solely 512-byte prefix, respects accepted-byte
  cap **prior to allocating body**, window, and cancellation. Does not accumulate
  media in RAM. Retains header limits, verified TLS, overall deadline, redirects,
  and stripping authenticated headers across origins. Distinguishes termination
  by window, cap, real EOF, timeout, truncation, and cancellation.
- Android/libcurl lacks system CA fallback. N01 reuses Mozilla bundle configured
  at boot via `rede_discord_ca`, unless explicit request CA provided. Does not
  disable TLS or fetch additional certificates.
- Diagnostics real `medirNucleo()` uses `streamfitdiag_medir()` when epoch is
  reliable. The requested test measures media via N01; zero additional HEAD on
  resolver, zero implicit retry. Preserves existing 416 retry from start. Uses
  existing duration/cap parameters.
- Bridge accepts solely 200/206, known `video/*` MIME, prefix without warning
  HTML/JSON, final host, and at least 5 complete intervals. Each throughput rate
  uses **real** interval bytes × 8 / duration; does not synthesize multiple
  seconds from aggregated transfer. Short final interval excluded; real stall zero
  included. Cap, transport failure, timeout, HTML, unknown MIME, cancellation, or
  differing epoch do not feed history. New English logs with counts/status/error,
  zero URL/headers/network/title logged.
- UI thread blocks test initiation during active player/mini; `app.c` cancels
  before each player opening and during active player/mini. Cancellation is a
  request, not a promise of synchronous socket/DNS termination.

## Files in this Slice

Production:

```text
android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt
src/app.c                     (diagnostics cancellation only)
src/diagnostico.c              (epoch, N01 route, playback guard)
src/diagnostico.h              (cancelar_vazao)
src/rede.c                    (CA bundle accessible to N01)
src/rede.h                    (additive fields discard/cancellation/result)
src/rede_pedido.inc            (N01 prerequisite; disposable mode)
src/redemarca.c                (new)
src/redemarca.h                (new)
src/streamfitdiag.c            (new)
src/streamfitdiag.h            (new)
```

Tests:

```text
tests/streamfit_diagnostico.c   (new; includes real medirNucleo)
tests/streamfit_diagnostico.sh  (new; zero whole-core build)
tests/streamfit_diagnostico_server.py (new; local HTTP)
tests/redemarca_android.py     (new; extracted real Kotlin + SDK 35 + JVM replay)
tests/rede_pedido.c            (N01 prerequisite; boot CA bundle test)
tests/rede_pedido_buffer.c     (N01 prerequisite; allocation-free discard)
```

`tests/rede_pedido.sh`, `tests/rede_pedido_server.py`, `tests/rede_retry_wgt.cjs`
and N01 files cited in `tests/rede_pedido.md` already belonged to prior uncommitted
delivery. **Do not stage full files indiscriminately**: `diagnostico.c`, `rede.c/h`
also contain earlier fixes; index and foreign agent files were preserved. Zero
changes in `player.c`, `ajustes.c`, `detail`, Home renderer, `streams.c/h`, `extras`,
or `ParaleloDataSource`/`NvPlayer`.

## Final Evidence

All PASS on final source, without core build or external network:

| Command | Evidence |
|---|---|
| `NV_SANITIZERS=1 bash tests/streamfit_diagnostico.sh` | `/tmp/nuvio-streamfit-diagnostic-final-asan.log` |
| `NV_TSAN=1 bash tests/streamfit_diagnostico.sh` | `/tmp/nuvio-streamfit-diagnostic-tsan.log` |
| `NV_SANITIZERS=1 NV_TPK40_TEST=1 bash tests/rede_pedido.sh` | `/tmp/nuvio-rede-pedido-discard-final.log` |
| `python3 tests/redemarca_android.py` | `/tmp/nuvio-streamfit-network-android.log` |
| `bash tests/rede_buffer.sh` | `/tmp/nuvio-rede-buffer-f03.log` |
| `bash tests/rede_reuso.sh` | `/tmp/nuvio-rede-reuso-f03.log` |
| `bash tests/rede_parada.sh` | `/tmp/nuvio-rede-parada-f03.log` |
| `bash tests/rede_sonda.sh` | `/tmp/nuvio-rede-sonda-f03.log`, includes WGT |
| `bash tests/diagnostico_persistencia.sh` | `/tmp/nuvio-diag-persistencia-f03.log` |
| `bash tests/diagnostico_dispatch.sh` | `/tmp/nuvio-diag-dispatch-f03.log` |

Also PASS on focused portion of `tests/vazao.sh` **before `sources=()`**:
math/scheduler and legacy transfer against local HTTP. Log
`/tmp/nuvio-vazao-legacy-focused.log`. **Full-core `vazao_fluxo` flow was not
executed in this slice**; root serializes that subsequent gate.

Real `medirNucleo` path followed redirect from 127.0.0.1 to localhost,
stripped synthetic private header, and recorded 6 intervals on final host,
including real silence zero. Resolver remained unknown. Tests also covered
503, truncated body, cap, deadline, stall cancellation, disconnection,
HTML, HTML with video MIME, and unknown MIME. Zero mock fixtures injected
into transport. Selection SDL clock is monotonic double to avoid AppKit
dependency; libcurl, engine, and bridge are real production code.

Network/bridge/epoch host syntax (`-Wall -Wextra -Werror`) and app/diagnostics
passed. NDK 27.2 arm64 and ARMv7 syntax with `NV_ANDROID` passed. TPK 40 test
compiled network and verified absence of compiler TLS. Kotlin compiled real
block against `android-35.jar` and reproduced route/link/caps/blocked, stale
notification, unregister, and monitor failure. Not an APK/TPK/WGT build or
proof of callbacks/latency on physical hardware.

## Limitations and Continuity

1. **Runtime not wired:** real duration per movie/episode ID and backend still
   missing. Consequently sheet may remain `SF_SEM_DURACAO`; do not announce
   end-to-end reordering ready. Existing `streamfit.c/h` and `streams.c/h`
   preserved. Next step: extras identity/provenance APIs for movie and real
   `CatEp`, then `video_duracao()` with `alvoPlayer` + player with video/ready/clean,
   never trailer or guessed duration. Not started.
2. **Android diagnostics uses single N01 connection:** conservative; not the
   legacy sum of 4 connections with unsynchronized seconds. Do not claim
   equivalence to parallel playback throughput or passive learning.
   `TransferListener` in `ParaleloDataSource` remains pending.
3. **LG and Samsung TPK:** without reliable network adapter, epoch is zero.
   Legacy speed test continues working; does not enter as StreamFit data.
   Do not deduce interface identity or address from HTTP responses.
4. **Samsung WGT:** synchronous XHR does not provide N01 contract; zero fake
   cancellation created. Legacy retains existing limits; zero ingestion.
5. **Media coverage is conservative:** missing MIME, octet-stream, and playlists
   remain unknown. Providers delivering generic video require additional format
   proof. Do not relax these responses via path extension.
6. **Redirects and budget:** N01 strips all caller headers when origin changes,
   including Range. Final host may respond 200 starting from byte 0; do not
   fabricate 5 MB offset measurement. Does not resolve hosts of all links in an
   add-on or generalize resolver. Cap bounds accepted bytes; libcurl may have
   received rejected block on wire. Zero large body allocated; timeout/DNS/cancel
   remain cooperative per N01 contract.
7. Native monitor invalidated immediately via JNI; account/profile sources use
   same physical network. In-RAM history only, zero persistence or measurement
   after reinstall/restart. Origin/age/unavailability UI and playback guard
   explanation must be integrated/reviewed subsequently.
8. Integrated full-core, packaging, and physical Android/LG/Samsung validation
   remain coordinator gates. Do not interpret installed Luna snapshot APK as
   proof of this code, which was written **after** the freeze.

Primary sources used for callbacks/abort contract:
[Android NetworkCallback](https://developer.android.com/reference/kotlin/android/net/ConnectivityManager.NetworkCallback),
[Android network state](https://developer.android.com/develop/connectivity/network-ops/reading-network-state),
[libcurl progress](https://curl.se/libcurl/c/CURLOPT_XFERINFOFUNCTION.html).
