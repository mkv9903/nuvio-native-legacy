# Social: TV <-> Server Contract (Redesign of 10/02/2026)

Server: `servidor/recomendacoes/src/social.js` (+ `index.js`). Tables:
`migracao-006-social.sql`. Client: `src/recomenda.h` ("REDESIGN" section).
Tests: `servidor/recomendacoes/teste-social.sh`, `tests/recomenda_social.sh`.

## Identity

- Standard headers: `Authorization: Bearer <token>` + `X-Nuvio-Auth: nuvio|trakt`.
- **New**: `X-Nuvio-Perfil: <profile_index>` only when active profile is NOT
  the primary profile. Person ID: `nuvio:<sub>:<index>`. Primary (without header)
  remains `nuvio:<sub>` — no legacy data changes ownership. Trakt ignores.
  Values outside 1..32 or non-numeric = primary.
- `POST /v1/eu` accepts `{"nome": "<profile_name>", "avatar": "<url>"}`
  (empty body remains valid). Avatar requires `https` from known host (host of
  `SUPABASE_URL`, Trakt, or the worker's `AVATAR_HOSTS` list); otherwise `""`.
  Response receives `nome` (resolved), `exibicao`, `alcance`, `perfil`.
- Resolved name (`pessoa.nome`): display name > profile name (Nuvio) / account name
  (Trakt) > `Friend #<rowid>`. Never the raw ID.
- `POST /v1/eu/nome {"nome": ".."}` — display name (letters, numbers,
  spaces, `'` and `-`, up to 32 chars). `""` clears and falls back to profile name.

## Privacy (Reach)

`POST /v1/alcance {"nivel": 0|1|2}`. `pessoa.alcance`: -1 unprompted (=0),
0 nobody, 1 friends only, 2 friends of friends. Checked on every read and
write. Level 0 clears events, "now playing" and aggregations.

Decision: **decoupled** from `descobrivel` ("who can discover me") and legacy
`perfil.ativ` (old route `/v1/amigos/atividade`). Reason: different questions;
and old `ativ` does not map to reach (new events — dropoff, reaction, duration —
exceed what that prior consent covered).

## POST /v1/atividade (Player Events)

Exact requested contract; **nothing changed**:

```
{"ev":"inicio"|"progresso"|"fim"|"abandono"|"reacao"|"salvo", "imdb":"tt..",
 "midia":"movie"|"series", "titulo":"..", "poster":"https://..", "temporada":0,
 "episodio":0, "pct":0-100, "seg":<s in this interval>, "reacao":1|0|-1, "rec":<id|0>}
```

Response: `{"ok":1,"guardado":0|1,"evento":0|1}` (`guardado:0` = reach < 1).
Server details:
- `progresso`: only updates "watching now" (15 min TTL) and aggregations; does
  not create an event entry. `inicio` also activates "now playing"; `fim`/`abandono` turn it off.
- **10/06/2026 — exiting player is no longer an event.** The new client sends
  `progresso` with `"parou":1` (closes "now playing" instead of renewing it);
  legacy client `abandono` does the same and also DOES NOT generate an `evento` row.
- Identical ev+title+episode+reaction within 10 min = single deduplicated event.
- `seg` capped at 4 h per event; `poster` only from known hosts (tmdb,
  metahub, amazon, tvdb, trakt), otherwise empty.
- `rec`: only modifies the rec if it was sent TO the sender
  (`inicio`/`progresso` -> started, `fim` -> completed, `reacao` -> reaction).
- Rate limit: 600 events/h per user. Retention: 90 days.
- **Legacy body** (without `ev`: `imdb,tipo,titulo,ano,nota,agora`) remains
  supported on the same route and routes to legacy `amigos.js` logic — live TVs
  and current `enviarAtividade` in recomenda.c ("Among Friends" row via
  `perfil.ativ`) rely on it. Route discriminates based on presence of `ev` field.

## POST /v1/rec/resposta (Direct Reply to Received Recommendation)

Migration: `migracao-007-resposta.sql` (columns `rec.resposta`, `rec.respondido`).
**Run migration BEFORE deploy** (`GET /v1/amigo` starts reading columns).

```
{"id": <rec>, "reacao": 1|0|-1|null, "texto": "a-z0-9 and space, up to 60"}
```

Response `{"ok":1,"n":0|1}`. Sets `comecou=terminou=visto=aberto=1` and writes
`respondido` (epoch). `reacao` null/omitted and empty text DO NOT clear existing
values. Only recs sent TO the responder can be modified (`n:0` for third-party ID).
DOES NOT check reach: it is an explicit interaction ("User will see your response"),
not automated background activity. Rate limit: 120/h per user. `GET /v1/amigo` adds
`resposta` and `respondido` to each item in `recs`.

Client: `src/recresp.c` (local state in `recomendacoes-respostas.txt`,
dispatched via worker thread in `recomenda.c`). Interaction triggers: "Already watched" in
Friends tab long-press OK menu, player completion (`atividade.c`, same "ended" signal as
rest of app), and "What did you think?" credits card (`reacao.c`, step 2: message).

### GET /v1/rec: "Watched" State Across Other Devices (W22)

No new migration needed: reads `rec.terminou`, `rec.reacao`, `rec.resposta`,
`rec.respondido` (migrations 006 and 007). Response (new fields in **bold**;
legacy clients ignore, modern clients tolerate absence):

```
{"cursor": N, "novas": N,
 "itens": [{... standard fields ..., "terminou":0|1, "reacao":1|0|-1|null,
            "resposta":"", "respondido":0}],
 "respostas": [{"id":<rec>, "terminou":0|1, "reacao":1|0|-1|null,
                "resposta":"", "respondido":<epoch>|0}]}
```

- `respostas` provides state for EVERY user recommendation that has been watched
  (`terminou=1`), replied to (`respondido>0`), or reacted to, including older IDs —
  `?desde=` only delivers new IDs, and a rec answered on another TV carries an older ID.
  Up to 120 items, newest first, omitting poster/title (TV already caches the rec).
- ETag updated to `"<n>-<highestId>-<unseen>-<replies>.<sig>"`: replying on
  another TV invalidates 304. First poll post-deploy returns 200.
- Client (`recomenda.c` -> `recresp_do_servidor`): merges into local state. Rows
  with unsynced local modifications (`version > sent`) are not overwritten;
  otherwise server only appends (watched/replied never revert) and server
  reaction/message wins if `respondido` is newer than latest local change
  or if local value is unset. Data from server is not re-broadcast.
- `GET /v1/amigo` (sender views response): `recs[].resposta` and
  `recs[].respondido` already existed; friend profile displays them (card
  "watched · liked / so-so / disliked" and, when focused, the message).

## GET /v1/feed?desde=<id>

Only actionable items (10/06/2026): `fim`, `reacao`, and `inicio` of the FIRST
episode in a series (started movie, subsequent episodes, `abandono`, and `salvo` are
omitted — older rows remain in table, but are filtered from feed). ONE ENTRY PER
USER AND TITLE: multiple `fim` events for a series collapse into one row with `"eps":<distinct_episodes>`
(only when > 1) and latest reaction for title attaches to
`fim` as `"reac":1|0|-1`. Filtered to accessible contacts, newest first, up to 50, with
`ETag`/`If-None-Match` (304). Client always queries `desde=0` (so lowering reach
immediately purges user from friend feeds).

```
{"cursor":11,"itens":[{"id":11,"de":"nuvio:aaa"|"pub:<handle>","deNome":"..",
 "deAvatar":"..","grau":1|2,"via":"<mutual friend, degree 2>","ev":"reacao",
 "imdb":"tt..","midia":"movie","titulo":"..","poster":"..","temporada":0,
 "episodio":0,"pct":0,"reacao":1,"criado":1790974381}]}
```

Degree 2 (friend of a friend, reach 2 only): ID masked as opaque handle, avatar omitted.
Block (either direction) hides activity. Limitation: ETag relies on max ID + count.

## GET /v1/amigo?id=<contact ID | pub:handle>

```
{"id","nome","avatar","grau","via","desde","origem":"codigo|trakt|sugestao|pedido|",
 "compartilha":0|1,
 "mes":{"mes":"2026-10","seg":2700,"filmes":1,"series":1}|null,
 "agora":{imdb,midia,titulo,poster,temporada,episodio,pct,atualizado}|null,
 "gostou":[{imdb,midia,titulo,poster,criado}] (up to 10),
 "recs":[{id,imdb,tipo,titulo,poster,criado,"estado":"entregue|aberta|comecou|terminou|reacao","reacao":1|0|-1|null}] (degree 1 only, up to 20),
 "gosto":{"total","iguais","pct"}|null}
```

404 for non-contacts and non-visible degree-2 friends (matches "does not exist"
response). `gosto` calculated only when both parties have reach >= 1. Empty `origem` =
contact established prior to migration.

`GET /v1/contatos` includes `desde` and `via` on each contact entry.

## Client Source Merging

`RecEvento` (recomenda.h) is unified structure. `rec_eventos_unir` deduplicates by
person + imdb + action (reaction also checks value) within |dt| <= 1 h, or whenever
one timestamp is missing; merge prefers our recommendation server, supplemented
by external metadata. `recomenda_feed_unido` merges feed cache with entries from
`trakt_social`. Trakt "watching now" = `REC_ACTION_START`, "watched" =
`REC_ACTION_END`, "rated" = `REC_ACTION_RATING`.

Simkl and Letterboxd: not implemented (see docs/social-sources.md). If owner
approves, UI input requirements: Simkl = NUMERIC friend ID; Letterboxd =
friend username (ideally provided by friend); and Letterboxd only
returns TMDB IDs (requires TMDB->IMDb mapping before deduplication).

## Integration with Player Module (agent/reacao, src/atividade.c)

- Permissions: on startup, `recomenda_ao_mudar_alcance(atividade_definir_permitido)`.
  Invoked immediately and on each reach level shift, outside mutex; unprompted = 0.
- Identity: `atividade.c` builds headers independently and **omits
  `X-Nuvio-Perfil`** — on secondary profiles, events would map to primary profile.
  In merge, replace its internal `identidade()` with
  `recomenda_cabecalhos(cab /*[4]*/, aut, .., via, .., perfil, ..)`.
- `recomenda_atividade()` (internal queue in recomenda.c) performs same role;
  with atividade.c merged, one is redundant — recommendation: retain
  atividade.c (features disk queue) and delete `recomenda_atividade`.
