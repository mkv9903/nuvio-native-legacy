# F08 — Social with Unified Identity (Contract)

Status as of 10/04/2026: implemented in repository (branch `f08-social-identidade`), **un-deployed**. Migration `servidor/recomendacoes/migracao-008-identidade.sql`, routes in `servidor/recomendacoes/src/identidade.js`, client in `src/recomenda.c`, `src/socialvis.c`, `src/amigoperfil.c`, and `src/salvospainel.c`. Prior contracts remain effective: [social-contract](../../social-contract.md), [social-sources](../../social-sources.md). Owner decisions (10/02): each household profile is an individual person; merge Trakt/Simkl/Letterboxd; privacy tiering.

## 1. Canonical Person

- The **canonical person** is a row in `pessoa`. With a Nuvio account, it represents the **profile**: `nuvio:<sub>` for primary and `nuvio:<sub>:<index>` for others (`X-Nuvio-Perfil` header, in production since migration 006). Zero new IDs created: existing entities retain identical ownership.
- Trakt-only users remain `trakt:<slug>` until linking a Nuvio profile. Social does not depend on Trakt or Simkl: with solely a Nuvio account, friends (code, suggestion, request), activity (`/v1/feed`), recommendations, and replies operate using profile ID.
- Trakt, Simkl, and Letterboxd are **linked identities** attached to person, never independent persons post-linking.

## 2. Linked Identities and Verification

Table `identidade (provedor, sujeito, pessoa, metodo, verificado, visivel, nome, criado, verificado_em)`.

| Provider | Subject | Verification Method | Effect |
|---|---|---|---|
| `trakt` | slug | OAuth token verified on the fly at `api.trakt.tv/users/settings` (same check as `quemE`) | Verified; authenticates and merges `trakt:<slug>` person |
| `simkl` | numeric account ID | Token verified at `api.simkl.com/users/settings` with `SIMKL_CLIENT_ID`. **Unverified against live API**; without secret, route returns 501 | Verified; zero merge (Simkl never represented person on server) |
| `letterboxd` | username (lowercase) | None: closed API. Stored as `metodo='declarado'`, `verificado=0` | Visible solely to user; never merges, resolves, or deduplicates |

Rules: linking requires **both proofs in identical request** — request authentication token and body token of second account, both verified synchronously at issuer and **never persisted**. Nothing merged by typed name, email, or slug. Verified identity belongs to single person (partial unique index); person holds at most one account per provider. Non-primary profile: index verified on Supabase with same token (`rpc/sync_pull_profiles`, resolving shared accounts); query failure fails closed (503).

### Routes

- `POST /v1/eu` gains `recursos: ["identidade1"]`, `autenticado: "trakt"|"nuvio"`, and `identidades: [...]`. Serves as client feature detection.
- `GET /v1/identidades` → `{pessoa, recursos, identidades}`.
- `POST /v1/identidades/vincular`
  - Trakt-authenticated request: `{"provedor":"nuvio","token":"<supabase>","perfil":<index, optional>}` → canonical = Nuvio profile; `trakt:<slug>` merged into it.
  - Nuvio-authenticated request: `{"provedor":"trakt","token":"<trakt>"}`, `{"provedor":"simkl","token":"..."}`, or `{"provedor":"letterboxd","usuario":"..."}`.
  - Responses: `200 {ok, pessoa, ja, fundiu}`; `409` identity of another profile or duplicate account for service; `401` invalid token; `403` profile does not belong to account; `501` Simkl lacking secret; `503` unable to verify profile.
- `POST /v1/identidades/desvincular {"provedor"}` — deletes link. **Does not split historical data**: merged data remains on canonical person; subsequent request from that Trakt becomes fresh empty person.
- `POST /v1/identidades/visivel {"provedor","visivel":0|1}` — verified identities only.

### Resolution

Every Trakt-authenticated request checks `identidade(trakt, slug, verificado=1)`; if found, handled **as canonical person** (Trakt name/avatar do not overwrite profile values). Legacy IDs stored by TVs (`para` on rec, `id` on remove/block, `?id=` on friend profile) translated via `fusao (de → para)` table prior to routing.

## 3. Deduplication

- **Friends**: post-merge, `trakt:<slug>` contacts rewritten to canonical (`INSERT OR IGNORE` bidirectionally), producing deduplicated server list. `GET /v1/contatos` returns `ids: ["trakt:<slug>", ...]` per contact — verified with `visivel=1` only, for direct contacts only. Client swaps `trakt:<slug>` on Trakt items for contact ID (`recomenda_feed_unido`), rendering single avatar/row.
- **Activity**: on client, `rec_eventos_unir` merges same person + title + action (+ episode, + reaction score) with |Δt| ≤ 1h; with swapped ID, event seen via Trakt and local server collapses to single row (local server wins). On server, `/v1/feed` collapses identical event from same person within 1h (two user TVs, one legacy on Trakt and one on Nuvio account). Different episodes and differing reactions never merge.
- **Channels**: Simkl and Letterboxd lack friend feeds (see social-sources); no channel to deduplicate yet. Structure (`ids` and `RecEvento.fonte`) accommodates future adapters.

## 4. Privacy

| State | Location | Meaning |
|---|---|---|
| `alcance` −1/0/1/2 | `pessoa` (006) | Unchanged: audience for automatic activity |
| `descobrivel` | `pessoa` (002) | Unchanged: appearance in suggestions |
| `identidade.visivel` 0/1 | 008 | 0 = private to user (server uses for merge/resolution); 1 = direct contacts receive ID in `/v1/contatos`. Default 1 for verified, 0 for declared. Currently API-only (zero UI) |
| Comparison | `/v1/amigo` | Only between mutually sharing users; otherwise client shows "Private" or "Enable activity to compare" |

Merge conflicts resolve toward **privacy-preserving state**: unconfigured (−1) adopts other side's response; if both answered, narrower `alcance` retained; narrower `descobrivel` retained (device reconciles local choice on subsequent `/v1/eu`); block overrides contact; canonical public handle wins; Trakt pairing code ceases to exist.

## 5. Coverage Comparisons

`GET /v1/amigo` → `gosto` returns `filmes {total, iguais}`, `series {total, iguais}`, `comum {filmes, series}` (titles completed by both within 90 days), `cobertura {eu, ele}`, and `generos: null`.

- **Match** = identical reaction (liked / so-so / disliked) among titles both users reacted to in last 90 days. Renders strictly with ≥ 5 pairs (`SV_CMP_MIN`); below threshold, "Insufficient data · n of 5". Sample size displayed alongside ("75% · 9 of 12 matching").
- **Movies / Shows**: same calculation, partitioned by media type.
- **Watched by both**: true count, no minimum.
- **Genres**: zero sources supply genre in events → always "No genre data".
- Explicit client states: Loading, Unknown (legacy server or absent server profile, e.g. Trakt-only contact), Private, Enable activity, Insufficient data. Never coerced to zero.

## 6. Migration of Existing Data

- Migration 008 is **strictly additive and idempotent** (2 tables, 3 indexes). Zero ownership reassignment upon execution; legacy worker functions normally alongside it.
- Merge occurs on per-person basis upon linking request. Batch transaction (D1) migrates contacts, requests, blocks, recs, events, "now", aggregates (summing monthly seconds), legacy activity, logs, and identities; deletes old person and records `fusao`.
- Test: `servidor/recomendacoes/teste-identidade.mjs` (full worker, real SQLite, mock issuers).

## 7. Compatibility

- **Legacy client + modern server**: continues preferring Trakt; post-link, requests route to canonical entity transparently. New fields (`recursos`, `ids`, `gosto` details) ignored. Shell test `"gosto":{"total":2,...` adapted for prefix matching (object expanded).
- **Modern client + legacy server**: lacking `identidade1` in `/v1/eu`, "Trakt on this profile" row omitted, zero `ids` expected, feed merge behaves as before, and comparison displays "Unknown" per media (Match displays if legacy server sends total).
- **Linking is explicit**: client never links automatically. On Friends tab, with Trakt and Nuvio account present on device and modern server detected, displays "Trakt on this profile · OK to link…". Unlinking requires two confirmations and warns that merged history remains.

## 8. Deployment (Pending, Requires Owner Approval)

Mandatory order:

1. `npx wrangler@4 d1 execute nuvio-recomendacoes --remote --config servidor/recomendacoes/wrangler.toml --file servidor/recomendacoes/migracao-008-identidade.sql`
2. Then `npx wrangler@4 deploy --config servidor/recomendacoes/wrangler.toml` **from a tree containing F08 and 006/007 routes** (deploying legacy master purges `/v1/feed`, `/v1/amigo`, `/v1/rec/resposta`). Migration 007 must already be applied.
3. Optional: `wrangler secret put SIMKL_CLIENT_ID` for Simkl linking (without secret: 501, client omits Simkl option).

## 9. Remaining Items

- UI for Simkl/Letterboxd and `visivel` toggles (API ready, client omits).
- Canonical person by shared account **owner** (`get_sync_owner`): currently ID uses token `sub`; two members of shared account on identical profile remain distinct persons. Modifying this requires additional merge logic (reusing `fusao` mechanism).
- Hardware unverified on TV and against live Trakt/Supabase/Simkl APIs: local SQLite, `wrangler dev --local`, and Mac GL captures only.
