# Media Servers — Proposal for Nuvio 1.8

Research from 10/03/2026, based on active official documentation and Native codebase. Status: specification; zero authenticated servers, private APIs, transcoding, or physical devices were tested in this task. Nothing implemented or promised as complete.

## Priority and Scope Boundaries

Proposal: Jellyfin first, Emby second, Plex third. Ordering stems from technical integration ease and server autonomy, not popularity rankings. Begin with movie/series libraries, detail views, seasons/episodes, version/audio/subtitle selection, playback, and progress sync. Music, live TV / DVR, offline downloads, server administration, watch parties, and remote control fall outside the initial increment.

| System | Role and Priority Rationale | Platform Specifics |
|---|---|---|
| Jellyfin | Personal server; clean initial HTTP/JSON adapter shared across TVs | Native login and Quick Connect; optional LAN discovery |
| Emby | Personal server; similar abstractions, but proprietary contract | User-level authentication; never replace with admin API key |
| Plex | Personal server with discovery/auth linked to plex.tv | Per-server tokens, decision/transcode engine, and distinct access policies |
| Kodi | Media center application with JSON-RPC | Local library/remote control and file paths do not match the 3 servers' streaming contracts |
| Finamp | Jellyfin music client | Not a 4th video server backend |
| SMB/NFS/WebDAV/DLNA | Underlying storage protocols | Do not provide equivalent metadata/progress/transcode layers; separate investigation |

Kodi exposes `VideoLibrary.GetMovies` and Player/Files namespaces via JSON-RPC; local `smb://` or `file://` paths obtained via RPC do not constitute streamable HTTP URLs on a TV. Finamp is officially designated as a Jellyfin music client. These distinctions avoid multiplying redundant or incompatible adapters. Sources: [Kodi JSON-RPC](https://kodi.wiki/view/JSON-RPC_API/v13.5), [Finamp](https://github.com/finamp-app/finamp).

## Unified Native Model

Implement proposed adapters `mediasrv.c/h`, `jellyfin.c/h`, `emby.c/h`, `plex.c/h` in future phases. HTTP networking leverages existing `rede.c` / JSON parsers, always executing on cancellable worker threads. Android can reference the official SDK; do not build an isolated Kotlin architecture that LG/Samsung cannot share.

Identities: `(type, serverId, userId, itemId)` for catalog/progress, `(serverId, mediaSourceId)` for stream version, and `playSessionId` for active playback. Never rely solely on IMDb/TMDB: files without external IDs or dual cuts represent distinct objects. External metadata enriches details; it never supersedes internal server identities.

Proposed new structs:

- `MediaServerConnection`: type, origin/base path, serverId, userId, deviceId, local credential reference, and observed version/capabilities.
- `MediaServerItem`: opaque ID, type, episode/series relations, int64 duration, images, optional external IDs, and remote watch state.
- `MediaServerPlayback`: item/source/session, ephemeral URL, headers, resolved playback method, tracks, start position, and transcoded timeline offset.

`streams.h` currently delivers URL4096 / headers512 / addon metadata. Never pack full authentication state into these fields or persist authenticated URLs as static source identities. An adapter resolves the stream URL immediately prior to playback and provides context to the player; headers require capacity and truncation audits. Preserve addon workflows and UI library boundaries: server errors must never wipe the addon library.

## Jellyfin — Initial Contract

Jellyfin provides HTTP 8096, HTTPS 8920 when enabled, and UDP 7359 subnet discovery. Base URL may contain reverse proxy prefixes; preserve `/jellyfin/` across all routes. Manual URL entry is mandatory and functions when broadcast is blocked. Discovery is a convenience shortcut, never automatic authentication. Source: [Official Networking](https://jellyfin.org/docs/general/post-install/networking/).

Recommended login: Quick Connect on TV, username/password as fallback. Official SDK specifies `authenticateUserByName` and `authenticateWithQuickConnect`, returning token/user. Passwords used only in initial request and discarded immediately; maintain local tokens per server/user. Source: [SDK Authentication](https://kotlin-sdk.jellyfin.org/guide/authentication.html). Quick Connect requires approval from an already authenticated client and may be disabled server-side; expiration/cancellation must cleanly stop polling loops. Source: [Quick Connect](https://jellyfin.org/docs/general/server/quick-connect/).

Pinned candidate schema routes:

| Operation | Candidate Contract |
|---|---|
| Server Identification | `GET /System/Info/Public`; verify ID/name/version prior to sending credentials |
| Password Login | `POST /Users/AuthenticateByName` with JSON body and client/device identification |
| Quick Connect | `/QuickConnect/Enabled`, `/QuickConnect/Initiate`, `/QuickConnect/Connect`, followed by `/Users/AuthenticateWithQuickConnect` |
| Library / Details / Episodes | Paged Items / UserViews / TVShows endpoints from pinned schema version |
| Playback Negotiation | `POST /Items/{itemId}/PlaybackInfo` with DeviceProfile and selections in payload |
| Playback Reporting | `/Sessions/Playing`, `/Progress`, `/Stopped` family from pinned schema version |
| External Subtitles | URL/track offered in MediaSource result; negotiate compatible format |

IMPORTANT: In the audited official `master` controller, Initiate is **POST**, Connect is GET; do not copy outdated tutorials assuming GET for both. `master` does not prove installed stable versions: pin and test against exact server releases prior to implementation. Never burst all auth verbs simultaneously. Source: [QuickConnectController](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/QuickConnectController.cs).

PlaybackInfo in the current controller accepts DeviceProfile in the request body, and several query parameters are deprecated. Byte throughput testing endpoint `/Playback/BitrateTest` and live source lifecycle endpoints exist, useful for future investigation; they do not authorize stress testing or live TV in the MVP. Runtime response selects media/URL and active session, rather than constructing transcode URLs via heuristics. Source: [MediaInfoController](https://github.com/jellyfin/jellyfin/blob/master/Jellyfin.Api/Controllers/MediaInfoController.cs).

## Emby — Decoupled Contract

API uses `http[s]://host:port/emby/{apipath}`, JSON supported. Interactive login must use user authentication; API keys are reserved for headless system integrations. Never reuse Jellyfin tokens or assume route/field parity survived project fork. Sources: [Emby API](https://dev.emby.media/doc/restapi/index.html), [User Authentication](https://dev.emby.media/doc/restapi/User-Authentication.html).

Negotiation: `POST /Items/{Id}/PlaybackInfo` with UserId, MediaSourceId, MaxStreamingBitrate, StartTimeTicks, audio/subtitle indices, DirectPlay/DirectStream/Transcoding flags, and DeviceProfile. This profile contains format, codec, and subtitle contracts; generate dynamically from verified device capabilities. Never copy generic browser profiles into the native player. Source: [Official PlaybackInfo](https://dev.emby.media/reference/RestAPI/MediaInfoService/postItemsByIdPlaybackinfo.html).

Send started/progress/stopped check-ins with ItemId, MediaSourceId, and PlaySessionId. Documentation recommends automated heartbeats every 10 s and immediately following user interaction; never POST per frame. Position uses integer ticks; marking watched is a separate independent operation. Source: [Playback Check-ins](https://dev.emby.media/doc/restapi/Playback-Check-ins.html).

Discovery / Emby Connect are subsequent increments: begin with explicit URL and documented authentication, avoiding assumptions of identical Jellyfin broadcast behavior. Verify permissions/terms and server license constraints prior to release; this research did not test paid Emby Premiere licenses.

## Plex — Contracts and Policies

Explicitly request JSON, as XML is default. Identify client with `X-Plex-Client-Identifier`; official PIN login includes legacy flow and documented JWK/JWT flows. Select a modern pinned flow, display code/QR, and cleanly terminate polling on expiration. Plex.tv master tokens and PMS tokens are separate: resources endpoint returns servers/connections/tokens; prioritize local LAN connections, then remote direct, using relay as last resort. Use returned URLs without disabling `plex.direct` TLS.

To stream, consume item metadata and call `/video/:/transcode/universal/decision` with profile/capabilities/quality, followed by indicated start resource or DirectPlay Part. Report timeline via `/:/timeline` per session; preserve media/part index and millisecond timebase. Never share tokens across distinct servers or infer transcoding solely from file extensions.

Source of operations: [Official Plex API](https://developer.plex.tv/pms/). Full documentation exceeded web tool limits; endpoints above were verified in indexed official excerpts. Pin OpenAPI schemas and review terms before implementation.

Remote personal video streaming requires Plex Pass / Remote Watch Pass depending on account/server administration. Handle entitlement errors as standard permission states; never spoof official clients to bypass restrictions. Relays carry a documented 2 Mbps ceiling; surface connection constraints accurately rather than misrepresenting them as TV Wi-Fi degradation. Sources: [Remote Access Troubleshooting](https://support.plex.tv/articles/200931138-troubleshooting-remote-access/), [Relay Service](https://support.plex.tv/articles/216766168-accessing-a-server-through-relay/).

## Playback and Device Capabilities

Unified strategy: DirectPlay when container/video/audio/subtitle and bitrate match verified local capabilities; DirectStream/remux when remuxing container/audio/subtitle resolves incompatibility; video transcoding only when necessary and permitted. Jellyfin documentation distinguishes these tiers and highlights subtitle burn-in overhead. Official client matrices do not guarantee Nuvio capabilities on LG C9 or Samsung. Source: [Codec Support](https://jellyfin.org/docs/general/clients/codec-support/).

Store capabilities per TV/backend: Android Media3/decoder/hardware and audio routing; LG uMS/firmware; Samsung WGT/AVPlay; Samsung native TPK. Nominal codec names are insufficient: profile/level, bit depth, HDR/DV metadata, interlacing, channel count, and passthrough dictate compatibility. Never falsely advertise unsupported codecs simply to avoid transcoding.

Apply conservative throughput caps to `MaxStreamingBitrate` based on measured server/network paths. Users may select original quality; indicate expected bandwidth and delivery method. Slow servers or saturated transcode queues do not prove slow home Wi-Fi. Prevent infinite fallback loops: attempt direct play, fall back to negotiated alternative, then surface diagnostic error with manual selection.

Verify custom HTTP headers across progressive streams and ALL HLS requests (manifest, segments, keys, subtitles), redirects, and byte ranges. LG/Samsung closed video URIs may require authenticated query params supported by the server; if local proxies are needed, demonstrate reliability, and never route tokens through public proxies. Never log secret URLs. On playback stop/switch, notify server and release transcode sessions immediately to conserve server CPU.

Distinguish server-delivered subtitles from embedded/addon tracks. Prefer external text subtitle extraction where supported to avoid forced burn-in; PGS/ASS subtitles require format testing. 1.8 AutoSync must not double-correct timelines: transcoded video starting at `StartTimeTicks` requires offsetting against item absolute time. Secondary subtitles and audio languages remain user selections, not justifications for silent transcoding.

## Watch State, Profiles, and Privacy

For personal server libraries, the server is authoritative for watch progress. Maintain local caches with full origin context and dedicated event queues; never route opaque server IDs into Nuvio addon progress tracking. Nuvio accounts and server users have explicit mappings: the same Nuvio profile can map to different server users across different TVs; never infer identity from matching usernames.

Events with generation tags (login/profile/item/session): started only upon active playback; progress with verified position, seek/pause state, and flags; stopped with final valid timestamp. Network dropouts preserve local queues and avoid blind replays in subsequent sessions. Resume calculates int64 seconds ↔ ticks against original runtime. Reconciliation compares event timestamps and never overwrites freshly committed progress with stale remote polls; never treat process-monotonic counters as universal clocks.

Tokens are scoped per device and server/user, never synced to Nuvio cloud accounts, telemetry, or M3 feeds. Never store passwords post-login. Android secure storage can protect tokens; LG/WGT/TPK local filesystems do not represent secure hardware enclaves and must be accurately documented. Logout wipes tokens, polling tasks, jobs, cached snapshots, and URLs. Logs use English redaction with type/method/status/latency, omitting secrets, private URLs, and full personal media titles.

Verify origin/basepath and restrict authenticated redirects: never rebroadcast tokens to arbitrary hosts. Enforce HTTPS by default; explicit local HTTP servers never receive credentials from other hosts. Changed ServerID or certificate requires deliberate reconnection, not automatic reuse of stale cache. Discovered URLs remain untrusted until verified. Authenticated images require identical header/cache policies and path sanitization.

## Validation Sequence

1. Pin OpenAPI schemas and verified versions across all three servers; build minimal public test fixtures without personal data. Audit SDK/client licenses prior to copying code; REST API consumption does not require embedding server source. Review proprietary Plex/Emby terms during implementation.
2. Jellyfin: login/cancellation, paged libraries, details, and DirectPlay movie on host, followed by 4 TV backends. Emby reuses UI/models with dedicated fixtures; do not implicitly reuse schemas.
3. PlaybackInfo, subtitles, resume, and check-ins; test remux/transcode and session termination. Plex follows after resources/auth and decision contracts match.
4. Integrate with Home and Continue Watching with distinct origin labels and isolated profiles. Coordinate with [player roadmap](../player-1.8/README.md) regarding bitrate, subtitles, and hardware capabilities.

Core test scenarios: URL/reverse proxy prefixes, spoofed discovery broadcasts, 401 token expiry vs. 503 network errors, QR login timeout/cancellation, large library pagination, non-IMDb IDs, multi-version titles, int64 ticks, unsupported audio tracks, PGS/ASS rendering, authenticated HLS redirects/ranges, transcode seek offsets, session teardown, offline queues, and profile switching during active requests. Verify logout deletion and absence of signed URLs in logs.

Per-device benchmarks across server versions: 10 cold/warm iterations, median/p95 startup latency, scroll/search responsiveness, seek/rebuffer times, app RAM footprint, and server CPU transcode load. Compare DirectPlay, remux, and transcode independently using identical media/hash/network conditions. Record delivered HDR/audio/subtitle tracks, not merely manifest metadata. Measure session network calls and absence of idle background polling. UI approval and TV hardware verification represent separate gates from desktop tests; this research does not satisfy hardware gates.
