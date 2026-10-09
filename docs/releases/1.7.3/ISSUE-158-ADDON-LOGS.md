# Issue 158 — Flix-Streams and Subtitle Add-ons

10/03/2026. Read-only review of 500 private records 21252–21751 exported from D1. Zero calls to private add-on URLs, zero authentication, public comments, or runtime modifications. Reporter identity was not established by dump, which lacks a user column; add-on combinations provide hints, not identification.

## Useful Evidence with Duplicates Removed

| Record/Line | Sanitized Evidence | Supported Conclusion |
|---|---|---|
| 21749:135–137 | Flix-Streams Free, manifest `org.flixstreams.free`, catalog=1/stream=1/subtitles=0 | Manifest was read and stream resource recognized |
| 21749:169–172 | Subs.ro Subtitles, `org.stremio.subsro`, catalog=0/stream=0/subtitles=1 | Subtitle resource recognized; zero catalogs expected for this type |
| 21749:191 & 224 | HTTP 404 on host associated with Flix-Streams Free | Two real 404 responses in app path, but redacted route/config prevents endpoint diagnosis |
| 21738:905–907 | Flix-Streams, `org.flickystream.addon`, catalog=1/stream=1/subtitles=0/meta=1 | Different implementation/manifest; cannot be treated as identical to Flix-Streams Free |
| 21738:541 | Subtitle search finished with 12 aggregated results | Not proof of Subs.ro or Community response; event does not indicate responsible add-on |

The 23 uploads with Subs.ro have byte-for-byte identical bodies (verified via local hash). Representative 21749: webOS, uploader 1.7.2, body 56,316 characters. Therefore they do not represent 23 independent sessions or 23 novel occurrences. This body does NOT contain a `[legendas]` event; does not prove HTTP query or Subs.ro parsing. Manifest presence does not prove subsequent usage.

PashaMetadata/PashaStreams and StremioCommunitySubtitles do not appear under those names in this slice. Absence of name does not prove user did not install them: bounded list, redacted/truncated logs, and manifests may use alternate names. Zero records were attributed to reporter solely because they contain Flix+Subs.ro.

## Why 404 Does Not Conclude Root Cause

`rede.c` passes URLs through `rede_url_publica` when logging status. Path and query become invisible, so log does not distinguish `/catalog`, `/stream`, `/meta`, or `/subtitles` on that host. Context is catalog/discovery activity, not playback confirmation. The 404 could be an incorrect route, declared yet unavailable catalog, incompatible ID, or server issue; one cannot choose among them with this evidence.

Redacted base text length 29/57 in log is NOT length of original configured URL and does not prove truncation. `addons.c` stores base length 600 and uses `url[900]` in subtitle searches, but complete private configuration is not in dump to measure limits or lost segment/query. Do not claim truncation fix for this case without a reproducing fixture.

## Concrete Client Code Points and Next Test

1. `addons.c:701–780`, `buscarLegendas`: queries active add-ons with subtitles flag, sequentially, 25s timeout per request. Failure or missing `subtitles` array generates zero per-add-on diagnostic; only aggregate summary at end. Add sanitized event with public manifest identifier, resource, item type, HTTP status, bytes, valid JSON / existing array, and accepted/filtered results. Never log movie ID, configuration, or URL/token.
2. The same loop builds `/subtitles/{type}/{id}.json` without applying specific `types`/`idPrefixes` declarations of subtitle resource. Suspicious for providers with proprietary IDs, but NOT proven as cause for Pasha/Subs.ro/Community in this dump. Fixtures must include manifest with object/type/prefix resources, series with season/episode, and empty vs invalid responses, alongside primary/secondary language and mandatory fields.
3. Base normalization in `addons.c:200` strips query and manifest suffix per legacy rule. Evaluate functional query vs version query and long base in fixture; do not alter global policy based solely on HTTP 404. Map manifests of each service to expected endpoints using public sources or explicitly provided test configurations, without invoking private URLs from this review.
4. Distinguish Flix-Streams Free (`org.flixstreams.free`) from Flix-Streams (`org.flickystream.addon`) in fixtures and report. Display name is insufficient key to deduplicate provider or apply targeted fixes.

Conclusion: Subs.ro resource recognition is proven; query/parser failure is not. There is a 404 associated with FlixFree with indeterminate endpoint. Investigation requires per-resource diagnostics and reproducible fixture prior to claiming issue 158 resolved.

## Reproductions and Fixes Following 1.7.3 Publication

In branch `codex/fix-addons-158`, commits `a8f8d031` and `354215a3`:

- Subs.ro maps `ro` to `ron` in its public implementation ([addon.js](https://github.com/allecsc/stremio-subs-ro/blob/master/addon.js)). Client only recognized `ro`/`rum`; Romanian preference rejected `ron`. Language family, on-screen label, and auto-selection now accept all three codes. Five test checks failed previously and passed with fix.
- Local preference "All" and absence of preference without secondary language created an exclusive English group in add-on search. Now use group without filter; chosen languages remain ordered primary, secondary, English, with identical shared result ceiling. Real worker test returned solely one English subtitle previously and now returns all four fixture languages.
- `tests/addons_legendas.sh`, regression `addonslista.sh`, language tests, and ASan/UBSan runs passed. Tests use synthetic local responses; do not access reporter configuration or verify their server response.

These fixes reside in local code and **are not part of published 1.7.3 packages**. Zero modifications made to release assets/tag, installation, or republishing. They do not demonstrate that all reported add-ons were resolved or establish cause of HTTP 404 in Flix-Streams.

Stremio protocol allows extras `filename`, `videoHash`, and `videoSize` ([SDK](https://github.com/Stremio/stremio-addon-sdk/blob/master/docs/api/requests/defineSubtitlesHandler.md)). Their absence was not accepted as universal explanation: Community supports routes without extras and Subs.ro supports optional filename. Preserve user configuration and compare real responses prior to altering this policy.

## Public Probe Without Private Configuration

On 10/03/2026, unconfigured public manifest [free.flixnest.app/manifest.json](https://free.flixnest.app/manifest.json) returned HTTP 200 and confirmed `org.flixstreams.free`. Resources: stream/catalog/meta; types: movie/series/tv; idPrefixes: tt/tmdb/essential/dlstreams. The two declared catalogs are tv/essential-live-events and tv/dlstreams-live. Main public manifest [flixnest.app/flix-streams/manifest.json](https://flixnest.app/flix-streams/manifest.json) returned 403 in this probe. Zero configured URLs, catalogs, streams, or media files queried. These probes do not identify add-on `org.flickystream.addon` or establish cause of historic 404 responses. Concluding Flix case requires fresh attempt log with resource/HTTP diagnostics or exact public manifest of installed add-on.
