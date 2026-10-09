# Social: Where Each Friend Data Point Originates

Research from 10/02/2026 for the Social redesign (Decision 3: "review all data
and assess what is needed"). **Measured** = verified via curl or inspected in
our codebase; **doc** = public API documentation claims; unverified on TV hardware.

## What Each Source Exposes About FRIENDS

| Friend Data Field | Trakt | Simkl | Letterboxd | Nuvio (Supabase) | Our Server |
|---|---|---|---|---|---|
| Friends / Following list | Yes: `users/me/following`, `/friends`, `/followers` (measured: `corpoSlugsTrakt` in recomenda.c, `socialPorSeguidos` in trakt.c) | **No** — no friends/following endpoint in API (doc: llms.txt on api.simkl.org, 127 pages, none covering friends/following) | No via RSS; closed API only | **No** — 13 tables, none social (measured 09/15, see index.js) | Yes: `contato` (invite code, Trakt, suggested, requested) |
| History / "Watched" | Yes: `users/:id/history`, feed `users/me/friends/activities` (measured: aggregated feed returns **401** on LG, comment in `trakt_social`; fallback per-followed user works) | Partial: `GET /users/recently-watched-background/{user_id}` = **1** title (doc), no list | Yes: RSS `letterboxd.com/<user>/rss/` returns latest **50** diary entries (measured 10/02: 50 items) | No | Yes (`evento`, 90 days) |
| "Watching Now" | Yes: `users/:id/watching` (measured in code, 8 followed users per cycle) | Own user only (scrobble); no third-party data (doc) | No | No | Yes (`agora`, 15 min TTL) |
| Rating / Like | Yes: `users/:id/ratings` (doc); feed includes `rating` action (measured in parser) | No for third parties | Yes in RSS: `memberRating` (0.5-5), `memberLike` Yes/No, `rewatch`, `watchedDate` (measured) | No | Yes: `reacao` 1/0/-1 |
| Watchlist | Yes: `users/:id/watchlist` (doc; private profiles require approved follower status) | Own user only; custom lists require PRO/VIP (doc) | No via RSS | No | `salvo` (event) |
| Aggregates (time, count) | `users/:id/stats` (doc) | `GET /users/{user_id}/stats` (doc: "most expensive API call", explicit user action only; requires **numeric user ID**) | No | No | Yes: monthly `agregado` |
| Episode / Season | Yes | — | Movies only (Letterboxd does not track TV shows) | — | Yes |
| Title ID | IMDb | IMDb/TMDB | **TMDB only** (`tmdb:movieId`), no IMDb (measured) | — | IMDb |
| Recs & friend interactions | Does not exist | Does not exist | Does not exist | Does not exist | **Ours only** |

## Authentication and Limits

- **Trakt**: User's OAuth token + `trakt-api-key` + User-Agent header
  (missing UA triggers 403, measured — see `idTrakt` comment in index.js).
  Authenticated GET rate limit: 500 to 1000 / 5 min per app+user (docs vary
  across versions; docs.trakt.tv specifies 500). Private profiles visible
  only to approved followers.
- **Simkl**: `client_id` sufficient for PUBLIC data of third-party user;
  private profile returns `403 private_profile` (doc). 10 GET/s per client_id;
  abusive traffic suspends client_id "without warning" (doc, api-rules).
  No mechanism to discover friend's numeric ID via API: user would have to enter it manually.
- **Letterboxd**: Official API is **closed** (requires email application; docs state
  they do not grant access to personal projects or "recommendation engines").
  RSS is public, requires no key, returns 200 OK without specialized User-Agent (measured).
  **Terms of Service**: explicitly forbids "robot, spider, scraper ... to access, copy,
  or **monitor**" service and export third-party content. Polling a FRIEND'S RSS
  feed every cycle to populate a feed is, under strict reading, "monitoring".
  Recommendation: **do not** enable by default; if owner approves, only with
  friend explicitly entering their username (consent from diary owner) and
  long cache TTL (>= 1 h).
- **Nuvio/Supabase**: Account token only validates identity (`/auth/v1/user`).
  User profile (name, avatar) resides in `profiles`, read client-side; server
  does not access it. Therefore, client sends profile name+avatar in `/v1/eu`.

## Conclusion

1. **Our server alone** supports: pure Nuvio account users (no Trakt), per-title
   reactions (liked/disliked), recommendation state machine (delivered/opened/started/
   finished/reacted), dropoff tracking, taste affinity between friends, monthly aggregates
   for users without external trackers, and friend-of-friend visibility tiers.
2. **Trakt** is the only viable external source providing a friend graph + activity +
   "watching now" suitable for TV, and is already integrated (`trakt_social`). Merged
   as `fonte=trakt`.
3. **Simkl** is unsuitable for a friend activity feed (no friend list endpoint, 1 title
   per user, high-cost stats endpoint). At most, a "last watched" look-up for a friend
   whose numeric ID is manually typed. Not implemented.
4. **Letterboxd** is technically trivial (RSS with rating, like, rewatch, timestamp),
   movies only and TMDB ID only (requires TMDB->IMDb resolution prior to deduplication).
   Blocker is legal ToS, not technical feasibility. Not implemented; the `RecEvento`
   struct accommodates `REC_FONTE_LETTERBOXD` if owner approves. UI would need
   to prompt for friend's Letterboxd handle — ideally supplied directly by friend.

Sources: api.simkl.org/llms.txt, api.simkl.org/api-rules,
api.simkl.org/api-reference/users.md, letterboxd.com/legal/terms-of-use,
letterboxd.com/api-beta, docs.trakt.tv/docs/rate-limiting, and curl against
letterboxd.com/dave/rss/ (10/02/2026).
