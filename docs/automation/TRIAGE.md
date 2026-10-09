# Automated Triage of Logs and Issues

Runbook for Claude scheduled job (cloud). It clones this repository,
reads new records in D1, crosses them with open GitHub issues, and publishes an
AGGREGATE summary. State between runs resides in the D1 `triagem` table
(`servidor/recomendacoes/migracao-004-triagem.sql`), never in git: the
repository is public and logs contain personal IDs.

## Environment

- `CLOUDFLARE_API_TOKEN` (D1-only permission, project account) and
  `CLOUDFLARE_ACCOUNT_ID`, as job environment variables.
- GitHub access via Claude app (issues and comments).
- Database: `nuvio-recomendacoes`. Commands from `servidor/recomendacoes`:
  `npx --yes wrangler d1 execute nuvio-recomendacoes --remote --json --command "<SQL>"`.
  Output contains non-JSON prefix noise: parse from the first `[`
  followed by a newline. Error 7403 is transient: retry after a few
  seconds. Query `texto` one record at a time (slow and heavy).

## Tables

- `registro(id, pessoa, versao, plataforma, quando, texto, criado)`: logs.
  - `quando` containing `(auto)`: periodic telemetry from active session.
  - `(anterior)`: previous session log, dispatched on each startup. Not a crash.
  - `(manual)`: sent manually by user, typically reporting an issue. Read
    these first.
  - `texto` beginning with `diagnostico=v2`: Diagnostic report, key=value formatted.
  - Actual crash: subsequent startup logs
    `[notices] previous session (...) did not exit cleanly`. On LG, clean session
    ends with `fim`.
  - Exclude `pessoa = 'trakt:iqui27'` (the owner).
- `triagem(criado, ultimo_log, padrao, versao, plataforma, ocorrencias,
  pessoas, issue, estado)`: state seen in previous run.
  - `estado`: `novo`, `conhecido`, `corrigido-em-X.Y.Z` or `regrediu`.

## Execution Run

1. `SELECT max(ultimo_log) FROM triagem` → read `registro` with `id >` this value.
2. Group by pattern, counting occurrences and unique users, partitioned by
   version and platform. Known patterns:
   - `Aborted(`, `RuntimeError`: WASM crash (Samsung).
   - `nao se despediu`: dead/unclean session.
   - `[video] avplay erro`, `errorText` not `"No Error"`: player error.
   - `[mkvass]` and `[legenda]` failure or fallback to TV: ASS subtitles (#92).
   - `montagem descartada`: inspect reason; `(addons)` and `(identidade)` are
     expected.
   - `padrao fora da lista`: reset setting (#129; fixed in 1.4.4).
   - `decode falhou` with path not starting with `/` or `http`:
     corrupted path.
   - `[debrid] ... downloading on TorBox`, `out of cache`, `PLAN_RESTRICTED`:
     TorBox/P2P.
   - `longtask-max=` above 2000 ms: major freeze (Samsung).
   - `[desc] ... excluded by quota`: catalogs trimmed by addon quota (#126).
   - In `diagnostico=v2` reports: count `aplicacao` values
     (`aplicada`, `ja_no_perfil`, `mantido_ruido`, `restaurada_auto`).
3. Compare against previous `triagem`. Pattern marked as `corrigido-em-X`
   reappearing in version >= X is flagged as `regrediu` (regressed).
4. Write one row per (pattern, version, platform) to `triagem`, with updated
   `ultimo_log`.
5. `gh issue list --state open`: map patterns to issues by number or subject.
6. Post comment on pinned issue "Log Report" (label `triagem`; create if
   missing):
   - aggregated metrics only, new patterns, regressions, and related issues;
   - never user IDs, log snippets with addon/debrid URLs, or tokens.

## Invariants

- Never commit logs, user IDs, or raw queries to the repository.
- Never reply to or close user issues: only comment on pinned issue.
- Never state "fixed" without a published release containing the fix.
