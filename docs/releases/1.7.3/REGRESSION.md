# Broad Regression 1.7.3

Executed on clean external checkout `2ddcc840`; final runtime `4ffb8a23` adds solely contrast adjustments to service cards, validated via PT/EN/glass screenshots and sheet tests. Packages were rebuilt on final runtime. Subsequent fixture and documentation fixes do not alter binaries.

Runner evaluated 290 scripts: 219 OK returns, 9 initial failures, and 62 excluded per runner rules. Raw return code does not equal full success when a test was skipped.

Consolidation following fixes and supplementary checks:

| Outcome | Scripts |
| --- | ---: |
| Full success | 225 |
| Partial (account: missing QR/OpenCV reader) | 1 |
| Not executed (recomenda_e2e: local service unavailable) | 1 |
| Pre-existing failure reproduced from 1.7.2 (detalheanime) | 1 |
| Excluded from suite by runner (targeted screenshots/diagnostics) | 62 |

Success total includes independent test `tpk40_tls.sh` executed by final Samsung packager: in suite tree it was skipped due to missing ARM binaries; in real build it passed, without TLS and with DT_HASH in TPK40, preserving common library TLS.

## Handled Initial Failures

- addonslista/fontecache_vod: isolated fixtures passed after provider discovery stubs, covered by their own test.
- contaoffline_limites: internal field reset was obsolete; fixture fixed and all limits/offline tests passed.
- salvos_segurar: fixed navigation in Settings already failed in 1.7.2; public setting search and Enter trigger passed.
- cachearte-wasm/entrada_texto_wgt/tizen-despedida: already installed Playwright dependencies exposed via NODE_PATH; full runtimes passed.
- tizen-globalthis: official Node 10.24.1 engine in network-isolated container tested real JavaScript of final package. Page/worker fail without polyfill and function with it; modern control also passed.

The detalheanime failure is the identical item type assertion from release/1.7.2 (exit 134), with zero differences in involved modules. The assertion was not modified to conceal the result. Consequently the suite is not represented as entirely green.

## Evidence and Limits

Logs: `/Volumes/ExternalSSD/nuvio-173-regression-suite.log`, `/Volumes/ExternalSSD/nuvio-173-regression-tmp/nuvio-testes.vSRoFG/` and `/Volumes/ExternalSSD/nuvio-173-host-checks/`. Screenshot scripts not executed by runner have separate evidence in VALIDATION.md. Node 10 / Playwright are test dependencies, not new app runtime dependencies.

No test in this report proves physical remote control, AVPlay firmware, real Netflix launch, or TV FPS gains. LG was unavailable and TCL sleeping; installation and hardware validation remain pending.
