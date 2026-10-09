# Local Preparation for 1.7.3

Base: `release/1.7.2`, `e1f2936b8695b90c9c9b545a2b2763d1bd33f83e`. Integration branch: `codex/release-1.7.3`, managed worktree `release-173-integration`. User request on 2026-10-03: prioritize log-derived fixes, Settings without provisional images, useful features still missing from scattered branches, and optional icons.

## Scope 1.7.3

- Remove provisional illustrations from Settings, retaining explanation, scope, and defaults.
- Audit access to legacy functions. The 1.7.1 enum does not lose options in 1.7.2: the catalog had 184 options, 61 advanced prior to outline access fix; now 60; the new icon option was appended at end without shifting persisted indices.
- Recover Where to Watch by hunks, preserving current partial sources, reconnect, and state. Regional list of TMDB/JustWatch services and opening installed app / store; does not promise deep links to titles or verify user subscriptions.
- Fix collections synchronization when RPC returns object and root array count is zero. Persist snapshot per profile and preserve on network failure.
- Include optional icons with existing supporter gate, provisionally via environment/file. No payment/Patreon integration in this delivery. LG/Samsung modify internal branding; Android schedules launcher alias/banner upon exit.
- Preserve Samsung player error code for diagnostics and source failure messaging. Previously log had code, but `video_erro_texto()` returned empty; does not by itself resolve provider/decoder error.

## Earlier Work Already Incorporated

Continue Watching patch `30b2c079` will not be reapplied: `player_encerrar` already invokes `desc_refazer_continuar` and `descoberta.c` preserves updated window across concurrent publications. Legacy Home, cache/profiles, and partial sources have more recent implementations in release: do not overwrite entire files with legacy versions. Current cache already prioritizes urgent artwork and protects recently used textures; do not backport 8-second protection without demonstrated benefit. Local Glass player stream UI changes belong in 1.8.

## Scope Reserved for 1.8

Social/Discord, Plugins/P2P, Glass UI, and the new AutoSync/audio bundle, secondary language, seek buffer, boost, network-based source tuning, Seekr 50/day and personal key, per-device performance profiles, diagnostics/speed test, and simplified subtitle selector. Specification: [../../plans/player-1.8/README.md](../../plans/player-1.8/README.md). Seekr personal key and limit remain grouped in that scope to avoid reopening usage without requested policy.

Personal media servers Jellyfin, Emby, and Plex also enter 1.8 investigation: [integration proposal](../../plans/media-servers-1.8/README.md).

## Publishing and Devices

This tree prepares a local candidate. Zero server changes, publishing, tagging, installation, or TV control are part of this round. Host Mac tests and compilation do not prove real LG C9, Samsung, or Android TV behavior. Separately log source, build, version, host/core, and scenario prior to validating hardware. `appinfo.json` is canonical version for Android and env; TPK scripts rewrite build manifest and restore original.
