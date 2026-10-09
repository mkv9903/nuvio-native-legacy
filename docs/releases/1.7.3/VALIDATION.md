# Candidate Validation 1.7.3

Local integration atop e1f2936, without publishing or installation on physical TVs.

Passed: Settings catalog/interaction, icon policy, collections and sync order, Continue Watching (order/removal), texture queue, per-profile settings persistence, trailer/hero contract, Samsung ROI/error diagnostics, Where to Watch parser with ASan/UBSan, source sheet and partial responses. Android: processDebugMainManifest and compileDebugKotlin. C# TPK 40/60/65 hosts: Release build with .NET 8.0.425.

New strings have English entries; existing languages were synchronized, using English for unlocalized entries. Scan distinguishes app identifiers from UI strings. i18n tests verify ordering, counts, and formats.

Host compilations are not installable packages or device proof. Physical LG C9, Android TV, and Samsung (WGT/TPK) validation remains pending, including remote navigation, app returns, and fluidity. Zero physical FPS or latency numbers are claimed from Mac tests.

Local execution logs reside in `/tmp/nuvio-merge-audit`; C# builds in `/Volumes/ExternalSSD/nuvio-173-host-checks`. Remote logs were queried read-only and are summarized in [LOGS.md](LOGS.md), without personal data or private URLs.

Host Mac fixture screenshots: [Settings without placeholder](imagens/settings-sem-placeholder.png) and [Accessible glass/outline](imagens/aparencia-contorno.png). These screenshots are not from a physical TV.

## Second Round: Services and Packages

Netflix and remaining services reside in the Where to Watch tab of the same source sheet. Installed-app inventory opens the real ID; missing offers store; informative Amazon/Apple channels did not trigger app/store in tests. Parser, region, offline errors, empty catalogs, delayed responses, and focus during partial sources were checked with ASan/UBSan.

Local packages generated and verified: [artifacts and hashes](PACKAGES.md). Full suite result recorded separately upon completion; skipped cases do not represent platform validation. LG did not respond to diagnostic probe; TCL was sleeping. Zero TVs awakened or updated.

Official references verified for Samsung discovery/launch: [Application API](https://developer.samsung.com/smarttv/develop/api-references/tizen-web-device-api-references/application-api.html) and [ApplicationManager .NET](https://docs.tizen.org/application/dotnet/api/TizenFX/latest/api/Tizen.Applications.ApplicationManager.html). Document verification does not prove firmware responses and permissions on physical hardware.

Final services screenshots: [Where to Watch](imagens/onde-ver.png) and [Where to Watch with glass](imagens/onde-ver-vidro.png). Offline fixtures on Mac, without logos or native launcher. Identified and resolved selected card contrast. Names and actions verified in PT/EN, with Netflix/Disney focus and glass mode.

Mock ARM Samsung host executed final core `4ffb8a23` with EGL/GLES llvmpipe: valid dlopen/config/context and 60 frame swaps completed, with login screen rendered. Command: `NV_HOST_LOCALE='' bash tools/tpk-testa.sh 60`. Without variable, legacy wrapper has undefined expansion; explicit definition worked around issue; wrapper was corrected to initialize when absent. QEMU/Mesa simulation does not measure FPS or Samsung firmware behavior. Login screenshot contained temporary pairing code and was not committed.

Final broad suite outcome: [regression and unvalidated cases](REGRESSION.md). Modified code tests passed; full suite retains pre-1.7.3 failure and skipped tests per report.

## Subsequent Android Installed-App Fix

[FIX-ANDROID-APPS.md](FIX-ANDROID-APPS.md) records runtime `0d7ae4e6`, additional concurrency/sanitizer tests, clean signed two-ABI rebuild, exact installed APK hash, and startup/query evidence on TCL. These checks supplement the earlier full regression rather than claiming entire suite was rerun on new commit.
