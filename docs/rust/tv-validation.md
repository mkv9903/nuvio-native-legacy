# Physical-TV hybrid validation — October 11, 2026

The user supplied two `space.mkv.nuvio.rusttest.log` files: a first launch
without login and a second, 915-line session exercising more of the app. After
reviewing the second log, the user confirmed movie picture, audio, subtitles
and navigation behaved normally compared with the current app.

The initial Rust runtime and targeted C/Rust hybrid smoke checks pass. This
is sufficient to begin a bounded logic migration, starting with add-on URL
helpers. It is not a full regression certification or evidence that all
future Rust dependencies work on the TV.

## Environment observed

| Item | Log evidence |
| --- | --- |
| App | Nuvio Rust Test, `space.mkv.nuvio.rusttest`, version 2.1.0 |
| TV | LG 32LK628BPTF |
| webOS / firmware | 4.4.3 / 05.50.70 |
| Kernel / architecture | `4.4.84-404.glacier.10` / `armv7l` |
| Kernel-visible memory | `MemTotal=641MB`; this is not a measurement of installed physical RAM or available memory |
| Graphics | Mali-470 MP, OpenGL ES 2.0, 1280x720 drawable |
| Writable data root | `.space.mkv.nuvio.rusttest` under the test installation directory |

## Successful checks

Line references below refer to the second uploaded log. Raw logs, account
identifiers and provider URLs are not copied into the repository.

| Check | Evidence |
| --- | --- |
| Rust runtime / C ABI | Line 4: ABI 1, `probe=PASS`, status 0, checks 7, checksum 522240. Exercises allocation, worker spawn/join, temporary-file write/sync/read/remove. Passed in both uploads. |
| Repeat launch | Line 7: second session, previous session exited cleanly. This does not prove post-login persistence across a later restart. |
| Login, profiles, sync | Lines 78–126: authenticated session, profile selection, synchronized add-ons/settings/progress and successful main-thread application. The early sync deferral ends after profile selection. |
| UI and navigation | Rendered first frame, remote events, title detail/source selection and playback controls; the user confirms normal navigation. |
| Native movie playback | Lines 503–575: 1920x804 HEVC MKV with E-AC-3 audio loads, enters `playing`, binds the video plane and applies its window. The user confirms picture and audio. |
| Subtitle selection | Lines 618–620: English SRT track selected through the TV subtitle path; the user confirms subtitles behaved normally. This is not coverage of ASS/SSA rendering or every subtitle codec. |
| Pause/resume | Lines 639–645: `paused` / `playing` events. |
| Seeking | Lines 650–675: seeks to 35s and 195s receive `seekDone`, with buffering followed by continued playback. A delayed-completion warning occurs on the first seek. |
| Aspect controls | Multiple source/destination changes are accepted by the native video window interface. |
| Teardown | Lines 908–915: quit event, player cleanup and `fim`; no Rust panic or process crash is logged. |

## Observations and follow-ups

1. **A background trailer fails.** Lines 452–481 show an Apple HLS trailer
   selected at 3840px with `hvc1` and no quality cap (`teto 0`), followed by
   webOS error 601, `Resource Allocation Error`. The movie selected afterward
   plays successfully. A codec/resolution or resource limit is a plausible
   explanation, not a confirmed cause. The trailer and native playback code
   are still C. Compare the same trailer/settings in the production app
   before attributing the failure to the hybrid build or fixing its cause.
2. **Frame stalls exist.** Sampled UI FPS ranges from 33.9 to 59.9. The worst
   logged frame is 435.9ms, largely in the GL clear operation; another
   283.4ms frame is largely in the update phase. App-process RSS ranges from
   32 to 67MB. These are observations from one short session, not a comparison
   against C-only performance, a memory-leak test or total media-pipeline RAM.
3. **A seek warning recovers.** The first seek reports a 3s completion warning
   before a `seekDone` event; the two completion messages report about
   2.7–2.9s. Exact timing should not be inferred from these interleaved logs.
4. **Three artwork downloads fail without a response body.** The log does
   not establish whether the cause is the provider, network or client.
5. **Two background diagnostic dumps fail to write.** Their filenames still
   use `/tmp/nuvio-fundo-*` in `background.c`. They should be scoped to the
   test app when the diagnostic isolation is next revised. No successful
   overwrite or production-data loss is evidenced by this log.

Focus loss/gain is logged near shutdown, but a complete background/resume flow
with continued operation is not independently established. Post-login relaunch
persistence, longer sessions, other media/subtitle formats and comparable
performance remain follow-up coverage rather than silently marked passed.

## Next migration

Add-on URL helpers now run in Rust behind the existing C callers, with
[automated parity and integration tests](addon-url.md). The uploaded logs above
precede that port and remain evidence for the initial bridge only. Use Linux as the primary
iteration environment, run the ARM cross-build/ABI checks, and repeat targeted
physical-TV checks when changing dependencies, platform integration or other
TV-sensitive behavior. Keep the remaining UI and playback in C during this
stage. Retain the observations above as reference behavior to compare against
future increments.
