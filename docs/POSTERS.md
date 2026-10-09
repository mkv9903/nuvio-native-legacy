# Custom Posters

Replaces portrait poster cards in the app (Home, Library, Search, "See all" and "More like this") with **pre-rendered** posters from an external service: textless art, logo, ratings, 4K/HDR/Dolby Vision badges, and Top 10 banner, all rendered directly onto the poster image itself. **Disabled by default.**

Settings > Layout > **Custom posters**.

Portrait cards (2:3) only. The hero spotlight on Home, the title page background, and landscape cards **do not change**: none of the services below provide 16:9 images.

## Providers

| Option | What it is | Requires |
|---|---|---|
| **SpatialPosters** | Open source server ([TheAceOfficials/SpatialPosters](https://github.com/TheAceOfficials/SpatialPosters), AGPL-3.0) that generates posters on the fly. Public instance or self-hosted. | Nothing (public instance) or your own server |
| **RPDB** | [RatingPosterDB](https://ratingposterdb.com), paid service with a limited free trial key. | Your account API key |
| **Custom template** | Any URL with `{imdb}`, `{tmdb}`, `{type}`, `{tipo_tmdb}`. | A server that responds with an image |

If the service fails to respond (HTTP error, timeout, corrupted image), the card falls back to the default poster and **does not retry in that session**. After 6 consecutive failures, the service pauses for 5 minutes, preventing an offline instance from causing a 20-second wait per card.

## SpatialPosters

### Public Instance

1. Settings > Layout > Custom posters > select **SpatialPosters**.
2. Leave address empty: defaults to `https://spatial-posters.vercel.app`.
3. **Test posters** downloads the poster for *The Shawshank Redemption* and reports success or failure.

The public instance is free and **shared** (see "Limits"). For daily use with many posters, self-hosting is recommended.

### Self-Hosted Instance (Docker)

```bash
docker run -d --name spatialposters -p 3000:3000 \
  -e SPATIALPOSTERS_TMDB_KEY="your_tmdb_v3_key" \
  -e SPATIALPOSTERS_PUBLIC_INSTANCE=1 \
  -v spatialposters_data:/data --restart unless-stopped \
  theaceofficials/spatialposters:latest
```

In the app: **SpatialPosters Address** = `192.168.1.5:3000` (IP and port of the server; without `https://`, becomes `http://` for IP and `.local` hostname). The default visual style (badge style, gradient, banner) is configured in your instance's studio — no token required.

### Choosing the Style: Short Parameters (Recommended)

**SpatialPosters Parameters** accepts the short URL query string, for example:

```
bs=vetro&side=right
```

Most useful parameters (native SpatialPosters names): `bs` badge style (`shadow`, `pill`, `bar`, `colored`, `bordo`, `vetro`), `rs` Top 10 banner style (`default`, `bar`, `colored`, `pill`, `netflix`), `side` (`left` or `right`), `badges=0` (no badges), `ranking=0` (no banner). Interface language is passed automatically (`lang=`). `fmt`, `format`, `config`, and `c` are rejected: format is always JPEG and token has its own field.

### Configuration Token and Addon Manifest

SpatialPosters also generates a **token** containing your complete configuration (`/c/<token>/…`). You can paste into **SpatialPosters Token** either the raw token or the full manifest URL (`https://…/c/<token>/manifest.json`): the app extracts the token and, if a host is present, also sets the instance address.

**Practical limit:** a full token often exceeds 500 characters, while the app accepts at most 400 (poster URLs must fit within 512 bytes in the image cache; typing on TV keyboard is also difficult). In practice, tokens are best suited for minimal setups. Prefer **short parameters** or **instance defaults**.

### Two Ways to Use SpatialPosters

**A) Image Endpoint (what this screen provides).** The app constructs the URL `<instance>/api/poster/{movie|series}/<tt…>?fmt=jpeg&lang=<lang>[&extras][&config=<token>]` for each title's IMDb ID. The title remains linked to your regular catalog: streams, Trakt, subtitles, and progress behave normally. This is the recommended mode.

**B) Stremio Addon.** Installing `https://<domain>/c/<token>/manifest.json` in your Nuvio account works **without this screen**: its catalogs supply the pre-rendered `poster` URL directly and the app renders it as received (hero and backgrounds still use the "Artwork source"). However, items from this addon carry `tmdb:<id>` instead of IMDb IDs, so stream/subtitle addons that only support IMDb IDs will not locate the title. Best suited for discovery rather than replacing your primary catalog. Both modes can be combined: with the addon installed and this screen enabled, posters in your other catalogs also adopt the custom style.

## RPDB

1. Obtain your API key from ratingposterdb.com.
2. Select **RPDB** and enter **RPDB Key**.

URL used: `https://api.ratingposterdb.com/<key>/imdb/poster-default/<tt…>.jpg?fallback=true` (with `fallback=true`, RPDB returns TMDB poster when custom poster is absent). Titles without IMDb ID use `…/tmdb/poster-default/{movie|series}-<id>.jpg`.
The image is a progressive JPEG of ~145 KB (580x859), larger than SpatialPosters (~48 KB, 500x750).

## Custom Template

**Posters URL Template** = an `http(s)://…` URL containing at least one of these placeholders:

| Placeholder | Value |
|---|---|
| `{imdb}` | `tt0111161` (titles without IMDb retain standard poster) |
| `{tmdb}` | Numeric TMDB ID (if unknown yet, retains standard poster) |
| `{type}` | `movie` or `series` |
| `{tipo_tmdb}` | `movie` or `tv` |

Example: `https://my.server/{type}/{imdb}.jpg`. Maximum 300 characters; final URL must fit within 512 bytes.

## Requests and Scheduling

- Movies and series only, with IMDb ID (`tt…`) or TMDB ID (`tmdb:…`). Live TV channels and non-standard addon items retain default posters.
- URLs are **stable** per title (do not change when the app discovers TMDB ID later): posters enter the unified on-disk image cache and are not re-downloaded on subsequent app launches.
- Maximum **3 concurrent downloads** from provider, even with 4 network worker threads available, with a 20 s timeout per poster (first poster request for a title is generated on server: ~3 s measured on public instance). A 200-card home screen populates progressively; cards display a gray skeleton placeholder until poster arrives (or default poster on error).
- Format: **always JPEG** (`fmt=jpeg`). Decodes directly on all targets (LG via libjpeg; Samsung in browser) and WebP showed identical file size in testing.
- Token and API key are **never logged**: URLs appear sanitized as `https://host/...`. Stored locally in `posteres.txt` in device data directory; not synced to account.

## Limits and Costs

- **Public SpatialPosters Instance (Vercel):** shared, no SLA. Server limits each IP to a bucket of 200 requests with 20/s refill (poster route) and returns `429`/`503` when exceeded; app handles this gracefully as item fallback. Cold posters are rendered in Vercel serverless function and can be slower. Completed posters are cached on CDN for 6 hours (`max-age=21600`).
- **Cost:** app charges nothing. Public instance: free. Self-hosted: your own server/NAS + free TMDB API key. RPDB: subscription tier.
- **Privacy:** chosen server receives TV IP address, title ID, and token/key (self-hosting is the most private option).
- **License:** SpatialPosters is AGPL-3.0. This app **only calls its HTTP endpoints**; contains no copied code. Self-hosters are subject to AGPL terms for server code.
- **Memory & Disk:** 500x750 poster decoded to card dimensions within standard texture budget.

## Common Issues

| "Test posters" message | Meaning |
|---|---|
| `success · 47 KB in 900 ms` | Everything working properly. |
| `no response from service` | Wrong address, server down, no internet, or 20 s timeout expired. |
| `responded, but not an image` | Server responds, but not with an image endpoint (proxy error page). |
| `incomplete or oversized configuration` | Missing RPDB key/template, or URL exceeded 512 bytes (long token). |
| `invalid token …` | Pasted text contains characters outside letters, numbers, and `_ . ~ = -` or exceeds 400 chars. |
