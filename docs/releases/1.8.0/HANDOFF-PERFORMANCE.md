# Handoff — Nuvio 2.0 (Formerly 1.8) Performance

10/05/2026. For a performance-specialized agent continuing operations. Read completely before proceeding.

## Where to Work

| Item | Value |
|---|---|
| Integration checkout | `/Users/hrocha/.codex/worktrees/integration-180-glass/nuvio-native-legacy` |
| Branch | `codex/integration-180-glass` (unpushed; public release is v1.7.4) |
| Reference HEAD | `ba1132a9` (+ this document) |
| Owner main repository | `/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy` — **do not touch** (branch `fix/tpk-selo-hdr`, owner work) |

Working guidelines, agreed with owner:
- **Never edit the integration checkout directly.** Create dedicated worktree on external SSD and merge afterwards:
  `git -C <integração> worktree add -b perf/<name> /Volumes/ExternalSSD/nv-<name> <HEAD>`
- Internal disk low on space: `export TMPDIR=/Volumes/ExternalSSD/tmp` and all outputs on SSD.
- Package builds require `export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"`
  (without this, package builds without login/addons/logging).
- Focused tests only ("fewer tests"). Full Mac compilation must stay clean:
  `cc src/*.c -o /Volumes/ExternalSSD/<name>-bin -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations`
- LG check (build only): `bash tools/arm.sh --build`. Deploy to TV **strictly upon owner request**.
- **Never reduce quality** (resolution, HDR, effects, animations) as "optimization". Only eliminate proven waste.
  Screen captures must remain identical (difference of few levels per pixel).
- Never claim root cause without proof or cite TV FPS that was unmeasured. Mac fixture uses Inter; TV uses **Montserrat**.
- English commits ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- The session merging into integration is "Nuvio 1.8 integration handoff". Coordinate before merging or opening overlapping tracks.

## Devices (Read-only, owner active)

| Device | Access | What to inspect |
|---|---|---|
| LG OLED C9 (webOS, Mali-G71, weakest GPU target) | `sshpass -p alpine ssh -o StrictHostKeyChecking=no -o ProxyCommand=none root@192.168.1.32` | `/tmp/nuvio.log`: lines `FPS=`, `[gpu-modos]`, `[perf]`, `[cor]`, `[tex-trace]` |
| TCL Android TV (Android 14, armeabi-v7a, Mali-G52) | `adb -s 192.168.1.128:5555 logcat -d -t 20000` | same lines in logcat |
| Samsung (WGT wasm / native TPK) | no device on bench | user logs only (D1) |

Never send keys, install, or create screenshot trigger files on TV without owner request.
Builds installed now: **LG = `ba1132a9`** (high-cache), **Android = `78ff220a`**.
Build recipe with preview label: worktree detached from HEAD, swap `1.7.4 → 1.8.0` in
`deploy/app/appinfo.json` and `tools/tizen-config.xml`, commit, `tools/release-android.sh` / `tools/arm.sh --alto-cache`.

## Measurement Tools

- `tests/fluidez_perf.sh` — counts **screens drawn per frame**, blended fullscreen layers, draw calls, per scene (Home in 3 layouts, detail, title page, Settings, Schedule, Library, Saved panel, menu, scenario `c9` = owner config). Repeatable metric.
- `FPS=` line (every 3s): `pior=`, `janks=`, `gpu-cache=`, `tela=` (hot set), `fila-tex=`, `tex-despejos=N(q=M)`, `disco-direto=`, `neg-arte=`, `cache-arte=… hit/miss`, `rss=`.
- `[gpu-modos] fill:` — screens per shader mode. Note: 320×180 ambient light bake counts as fullscreen on this line (overcounts).
- `[perf]` — timings for `cat_definir_tudo`, publication, snapshot, and saving catalog cache; lock waits above 8 ms.
- Samsung: `[navegador] … quem=<atribuição>@<fase>` (phase = boot/catalog/player/home) on long tasks.
- Boot timeline `[t]` (assembly, CW ready, manifests, first catalog, publication).
- Harnesses: `tests/trakt_arranque.sh` (simulated latency), `tests/catpub_bench.c`, `tests/logofila.sh`, `tests/texb3.sh`, `tests/fundo_assado.sh` (`gles` uses ANGLE).

## Real User Telemetry

`/Volumes/ExternalSSD/nv-auditoria/LOGS-ANALISE.md` (D1 `nuvio-recomendacoes`, `registro` table, 09/22–10/05,
1,163 sessions, 364 users; SELECT only, never write). Issues matrix: `ISSUES-MATRIZ.md` in same directory.
Core performance findings:
- Hero artwork missing on deadline: LG 96 users, WGT 63, TPK 52, Android 21.
- Network queue wait for artwork longer than download (LG p50/p90/p99 0.6/2.4/10s).
- FPS: 76 users on LG with median < 45 fps (more so on webOS 4); WGT is slowest.
- Long tasks on WGT: > 2s for 50 users, > 5s for 19, up to 22s.
- Sessions ending without clean exit: TPK 30, WGT 25, LG 17, Android 5.

## Completed in this Operation (Integration, verified Mac-only unless noted)

| Track | What Changed | Metric |
|---|---|---|
| B1 Rendering | Solid panel shadow drawn on edges only (`gfx_sombra_vazada`) | Settings 3.62 → 2.92 screens/frame; Schedule 1.48 → 1.13 |
| B2 Boot | Title records on disk (`fichameta.c`, TTL 6h); slow manifest does not block cycle (6s); parallel social feed | Trakt on boot 8.9 → 5.1s cold, 0.7s warm (Mac, simulated latency) |
| B3 Textures | Disk artwork decodes directly; 404 artwork cached 24h; upload > 2 MB in chunks within 4 ms/frame; logo/hero prioritize | On C9 `disco-direto=` appears; before: 150s combined queue for disk artwork, 8.3 MB hero in 14–58 ms |
| B4 Samsung | Compact catalog cache (26.9 → 1.9 MB at 1,600 titles), encoding outside FS lock; `[perf]`/phase markers | Unmeasured on hardware |
| Android Logos | Logos bypass network and decode queues; focused card logo requested | Simulation: wait 1.2–4.5s → 85 ms |
| Negative Cache | Failing endpoints (introdb, tiffara, cinemeta, tmdb) and Trakt 401 stop repeating | — |
| Frost / Blurred Art | Baked at 320×180 + 1 opaque quad (`1bcd6ebe`) | Title page 6.3 → 2.3 screens/frame |

Measurement on C9 post `ba1132a9` deploy (owner navigating): stable 49.5 FPS, worst frame ~30 ms, 0 janks,
gpu-cache 293 MB of 300, `tex-despejos=0`.

## Open Items (Priority)

1. **Frost and Blurred Art Incorrect on LG** (bake regression). On C9 self-check passes
   (`[cor] fundo assado conferido`), but on screen Blurred Art is purely dark (missing art) and Frost overly dark.
   Agent running on `fix/frost-ambiente-lg` (`/Volumes/ExternalSSD/nv-frostamb`): reuse Dynamic Home ambient light
   pipeline (lightweight and correct on C9) and single background dump in `/tmp/nuvio-fundo-<type>.bmp`.
   Coordinate before modifying `src/gfx.c` / `src/fundo.c`.
2. **Detail Carousel on C9**: TV measures 7.4 screens/frame with 7 fullscreen layers (3.3 solid color,
   2.5 carousel artwork); Mac harness reproduces only 3.0. Identify discrepancy between TV and fixture.
3. **Samsung WGT Long Tasks** (2–22s): primary suspect (unproven) was catalog persistence + `FS.syncfs`;
   reduced 14×. Requires real log with `quem=…@fase` to confirm.
4. **Android: 16s Upload Stall** during system low-memory killer activity — needs handling for
   `onTrimMemory` (release texture cache / pause prefetching).
5. **Small Artwork Decoded Large**: avatars, add-on and studio logos decode up to 640 px and render at
   40–200 px (~1 MB saved each). Decode at rendered size × scale, never larger.
6. **Samsung `ADD_FIOS` = 2** (12 on others): add-on queries queue behind 2 threads. Modify only with Samsung testing.
7. **"First Row Rendered" Marker** on boot: required to measure publish → paint in the field.
8. LG webOS 4 with median < 45 fps: GPU profile and layers per scene on these displays.

## Delivery Requirements

Dedicated worktree, focused tests, clean compilation, `arm.sh --build`, before/after captures with Montserrat
(on SSD), before/after numbers in identical scene. Concise report: measured root cause, fix, numbers, TV requirements.
