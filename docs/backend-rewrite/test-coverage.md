# Account and profile verification

Reference: `main` at `eff1a0b9`, archived locally under the ignored
`build/backend-reference` directory before implementation changes.
Reference tests run against fake transports or a local HTTP fixture. No real
account credentials are used by these tests.

| Existing C coverage | New Rust coverage |
| --- | --- |
| `sessao_email` password request, escaping, service errors and offline retry | `service` request/Serde/error cases; `account` transport failures; HTTP fixture added with UI integration |
| `sessao_fora` outages preserve credentials; actual refresh rejection clears them | `account` outage, retry and rejection cases |
| `sessao_corrida` concurrent RPC/table refresh and private token readers | `account` shared rotating refresh and concurrent readers |
| `sessao_parcial` malformed/truncated saves, anonymous sessions, malformed authentication and TLS failure | `storage` migration cases; `service` authentication cases; `account` TLS outage cases |
| `perfilsel` profile fields, selection rules, startup cache, PIN outcomes, primary-addon inheritance and logout | `storage`, `service` and `account` profile cases |

All five reference groups passed before integration. The initial Rust checkpoint
passes 26 tests. Additional coverage includes late replies after cancellation or
logout, stale refresh against a new account, disk-write failures, schema identity
and version validation, isolated migration, private atomic writes, configuration
key completeness, unverified PIN locks and shared-account owner handling.

Agreed corrections to C behavior: keep confirmed cached owners and locks during
outages; do not substitute the signed-in member for an unknown account owner;
block selection of a profile whose lock has not been verified. Primary-addon
inheritance uses the profile marked primary instead of assuming index 1.

Remaining checks for the completed capability: real HTTP transport fixtures,
worker scheduling, native boundary ownership, Linux C UI navigation and visual
comparison, ARM build/link and runtime-library inspection, isolated package
build, and the first user TV test.
