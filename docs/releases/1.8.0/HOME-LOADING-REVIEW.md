# Progressive Home — Independent Review

Read-only review of `src/catalogo.c/h:cat_home_apenas_fixas`, decision in
`src/descoberta.c:montar` and new fixture `tests/home_progressiva.c/sh`.
Without edits or duplication of the suite executed by the implementer.

Verdict: no new blockers observed in the hunks. The old decision `cat_n() == 0`
did not recognize the initial Home when CW/social arrived before the first
catalog; the new getter recognizes this state with coherent reading of items and
rows. The playback log previously had `ready=1ms`, `visible=354ms` waiting
for the slow catalog. The "after" result should come from implementer tests.

- Mutex `pubTrava` covers joint snapshot `n/itens/fils/nFils`, avoiding observing
  `n == 0` or intermediate windows during publication. The getter does not call APIs
  that acquire other locks and does not retain pointers after releasing the lock.
- Only reserved keys `continue_watching`/`social_activity` allow cold state.
  Any catalog/collection already published is warm, including a row with state
  but without items. `naLista`/`naColecao` markers and items not assigned to a window
  also block, preserving ready lists/metadata before classification.
- Warm exits during a short row scan; cold has at most two fixed rows and
  few items. It does not introduce heavy per-frame scans; called once per cycle.
- List state prepared within the cycle itself remains attached by existing
  publication helpers. `FonteIntacta`/generation guard publications
  per catalog and final discard/restart as before. Hunk does not relax identity.
- Tests use real discovery/catalog/home-state implementation with mocked network
  and controlled delay. Cases 0-3 separate cold fixed rows, warm catalog, ready watchlist
  without window, collection key; case 4 switches owner + generation during network.

Relevant boundaries: case 4 uses the same IDs in both owners and fixed profile 1; proves
cancel/restart, not distinct data between real profiles/accounts. Getter decides
once at startup: subsequently merged data follows existing protocol,
not a classification + publication transaction. Partial override on screen and
initial CW publication are inherited paths without guard alteration by the hunk;
it is not a comprehensive new race test for all publishers. TSan,
external provider, or TV were not run in this review. Implementer PASS logs and
integrated copy from root are complementary dynamic evidence.
