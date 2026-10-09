# N3 — Poll in Clock Island (Nuvio 2.0)

Approved mockup: `mockups-20/04-enquete-ilha.html`. Built strictly using capabilities the island already
has (pill, prompt with blue key, modal expanding from pill, `ilha_acao` with
Undo, `ilha_aviso_pediu`). Nothing was published: **no deploy, no migration applied,
no poll seeded in production.**

## Client

| File | What it does |
|---|---|
| `src/enquete.c/.h` | State, network thread (queue of 4, single thread), invite once, badge dot, vote, result, opt-out, and mirror. |
| `src/ilha.c/.h` | `IlhaModal.resultado/pct/escolha` (result row with 6px rail), `ilha_ponto_enquete` (accent dot on clock), and `ilha_ponto_pediu` (BLUE on idle clock). Modal title that overflows wraps to 2 lines. |
| `src/ilhasinais.c` | Delivers buttons for `enquete:*` keys and calls `enquete_passo` per frame. |
| `src/ajustes.c` + `ajustes_ux_*.inc` | `AJ_ENQUETES` ("Receive polls", enabled by default) at END of enum and parallel arrays. Located in **Accounts and services > Notifications** (Notifications section previously did not exist). |
| `src/app.c` | `enquete_perfil_trocado()` upon profile switch. |
| `src/idioma_*.h` | 18 new keys, 28 languages (`tools/idiomas.py` passes). |

Flow: initial `GET /v1/enquete` fires 15s after app settles (dedicated thread,
never blocks startup; repeats every 6h and on every profile switch). New and
valid poll: short notification "There is a poll for you" that auto-opens into invite modal
(Answer / Not now / Do not receive more polls). The invite appears **once per
poll and profile** (`enquete.txt`). "Not now" or Back leave the dot on the
clock; pressing BLUE on idle clock opens directly to options (with badge dot active,
BLUE on idle clock does not open hub — it returns when poll is
answered, expires, or opt-out is activated). Options = buttons (up to 3), 480x270 artwork from
server, deadline in footer. Single vote; modal then transforms into results (text,
percentage, rail, total votes). Already voted on another TV: no invite or
badge dot. Expired deadline: disappears.

Opt-out: invite button and Settings option send choice to **account**
(`POST /v1/enquete/optout`) and to local mirror (`enquetesLocal`). Via island there is
Undo (`ilhaacao`). If action occurred offline, `enquete-optout.txt` records
pending change and overrides mirror until server confirms; without pending change, server
takes precedence (another TV changed it).

Profile switch: increments generation; in-flight requests are dropped, queue and mailbox
cleared, invite and badge dot disappear, and new profile refetches.

## Server (`servidor/recomendacoes`)

- `migracao-009-enquete.sql` — `enquete`, `enquete_opcao`, `enquete_voto`
  (primary key `enquete, pessoa`: one vote per person), `enquete_optout`. Additive
  only, idempotent, **seeds nothing**.
- `src/enquete.js`, wired into `src/index.js` following authentication (same as all
  Social routes, profile via `X-Nuvio-Perfil`):
  - `GET /v1/enquete` -> `{optout, enquete}`; `enquete` is active
    (`ativa=1 AND inicio<=agora<fim`) with `id, pergunta, arte, fim, opcoes[], voto`.
    Only those who voted receive `resultado[]` and `total` (aggregated counts; never IDs).
  - `POST /v1/enquete/voto` `{id, opcao}` -> 200 (idempotent on same option),
    409 different option, 410 ended/future, 404 unknown, 400 invalid.
  - `POST /v1/enquete/optout` `{optout:1|0}` -> `{optout}`.
- Tests: `node --test servidor/recomendacoes/teste-enquete.mjs` (in-memory SQLite
  against entire worker, no network).

Create a poll (operator, outside migration; example for owner to run when
desired — **not executed**):

```sql
INSERT INTO enquete (id, pergunta, arte, inicio, fim, ativa) VALUES
  ('logo-2026', 'Which logo do you prefer?', 'https://.../logo-480x270.png',
   strftime('%s','now'), strftime('%s','now') + 7*86400, 1);
INSERT INTO enquete_opcao (enquete, idx, texto) VALUES
  ('logo-2026', 1, 'New'), ('logo-2026', 2, 'Refreshed Classic'), ('logo-2026', 3, 'Classic');
```

## Deployment Order (None of this was performed)

1. **Migration first**, from repository root:
   `npx wrangler@4 d1 execute nuvio-recomendacoes --remote --config servidor/recomendacoes/wrangler.toml --file servidor/recomendacoes/migracao-009-enquete.sql`
   (old worker continues working: does not know about new tables).
2. **Worker next**: `npx wrangler@4 deploy --config servidor/recomendacoes/wrangler.toml`.
   New worker against database without 009 only breaks `/v1/enquete*` (500), not remaining routes.
3. Only then insert poll (SQL above) via `d1 execute ... --remote --command`.
4. Client: without active poll nothing shows; older TV ignores feature.
Rollback: `UPDATE enquete SET ativa = 0`.

## Tests and Captures

- `bash tests/enquete.sh` — client against mock server in C
  (`tests/enquete_servidor.inc`): wait before first request, invite once
  (and after reopening app), badge dot, single vote, aggregated result, voting failure,
  expired deadline, opt-out with Undo and pending state, mirror from another TV, profile
  switch with in-flight request.
- `tests/ajustes_ux_dados.c` (last option = `AJ_ENQUETES`), `tests/ajustes_ux_interacao`.
- `bash tests/enquete_shot.sh <prefix>` — GL captures (Montserrat) via real path.
