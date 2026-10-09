# Local Packages 1.7.2

**Subsequent inclusion:** webOS screensaver `8a49956` integrated via `857f83e3` and adjusted in `11835ffa`. Current code `ab281e99`; the batch below from `b71471bc` does not include the fix. It remains technically verified, but is an intermediate artifact for the expanded release candidate. Repackaging pending post-freeze.

Previous technical suite completed, without publishing or installation in this round. All packages derive from frozen source `b71471bc20e36806de9665145e4898c5e8e67884`, including the durable add-on queue and the island card identity fix. Development in `/private/tmp/nv-172`, branch `release/1.7.2`; any subsequent documentation commit does not alter the compiled binaries.

## Current Batch

Attachments directory: `/Volumes/ExternalSSD/nuvio-rel-172/final`. Contains 12 product/metadata files and `SHA256SUMS`, totaling 13 attachments. Compilation, verification, and all 12 checksums passed. Evidence manifest outside attachments folder: `/Volumes/ExternalSSD/nuvio-rel-172/FINAL-MANIFEST.json`.

- Android: version 1.7.2/10702, expected pinned certificate, two ABIs with six libraries each; effective configuration verified strictly for presence and zero personal/snapshot/temporary files.
- LG normal/highcache: version 1.7.2 in manifest/control, ARM32/libass and effective configuration present; highcache marker present only in highcache build. Homebrew metadata verifies hash/size of normal IPK.
- Samsung: WGT and four 1.7.2 TPKs; two attached cores identical to packaged cores. Tizen 4/5 without PT_TLS/TLS relocations, with DT_HASH and without drminfo dependency; configuration/WASM provenance and Chrome69 compatibility of WGT verified by script. Full ZIP listings checked for zero personal/snapshot/temporary files. WGT distributed unsigned; installer signs with the TV's own certificate.
- Android/LG/Samsung build copies on external SSD, clean and from identical source. No TV was controlled to generate this evidence.

Local reports without configuration values:

- `/Volumes/ExternalSSD/nv172-b71471bc-android-verification.json`
- `/Volumes/ExternalSSD/nv172-b71471bc-lg-verification.json`
- `/Volumes/ExternalSSD/nv172-b71471bc-samsung-verification.json`

Logs: `/Volumes/ExternalSSD/nv172-b71471bc-android-build.log`, `/Volumes/ExternalSSD/nv172-b71471bc-lg-normal-build.log`, `/Volumes/ExternalSSD/nv172-b71471bc-lg-highcache-build.log` and `/Volumes/ExternalSSD/nuvio-rel-172/build-samsung-b71471bc.log`. Consolidated verification executed by `verify-final-b71471bc.py`, alongside manifest.

| Attachment | Bytes | SHA256 |
|---|---:|---|
| `Nuvio-1.7.2-NuvioTpk.tpk` | 27489770 | `d648e295306db06adea2508231bd9820063227e2aa8dc9bdeabd862814a422a4` |
| `Nuvio-1.7.2-NuvioTpk40.tpk` | 27494227 | `667a0c290b3943ccb24fb8a24c558e1ecf3a9d72f40cf546021c9612f33b1d1d` |
| `Nuvio-1.7.2-NuvioTpk60.tpk` | 27488809 | `b0834d29d166713a3847071ad6e30f3d654f219dd75f86167187b878afa42dee` |
| `Nuvio-1.7.2-NuvioTpk65.tpk` | 27488999 | `6c4242e32091ae6af8b6cedc4d0dc8f453e9d7a3a259277276626a7874787182` |
| `Nuvio-1.7.2-android.apk` | 43558895 | `f876c7dd44418d224826905c9a46148424f200281e59b6add08c8d0c33c4740f` |
| `NuvioTV-1.7.2-tizen.wgt` | 27097119 | `b760399947a36804e85e7e9b3c0f8bf361eea885328c219e8667e315931621bc` |
| `libnuvio-1.7.2-tpk-arm.so` | 9863240 | `a448c376baa8fef0d3dd3bb0e6590126e43609783daf35da492530a0658944c8` |
| `libnuvio-1.7.2-tpk40-arm.so` | 9867320 | `9e4f1d7d46d23bc13c49fd3c83ba1fdd7c3be1abda6f7b49324106a84a0de834` |
| `repo.json` | 1310 | `f2236e5e040791ded28a6e3aaaa8465c66ebaa963a08dc9a405c0db17686becf` |
| `space.nuvio.native.legacy_1.7.2_arm-highcache.ipk` | 51056670 | `44836dfc39b459b51903bb46cce350357ff6e585d1bbdf1f7bf8f56a156b894a` |
| `space.nuvio.native.legacy_1.7.2_arm.ipk` | 51056254 | `edcb17a9393f85363e4ee14b5b1e45c5a0062a923da7ec1ce3f09c6d434b42a4` |
| `webosbrew.manifest.json` | 633 | `d56446fa0e1fc45dcbf596ec97b19a28b4ac1ce5be83cbad4715a67e9e699813` |

The thirteenth attachment is `SHA256SUMS`; `shasum -a 256 -c SHA256SUMS` verified all 12 files. Logs, reports, and configurations are not public attachments.

## Preserved Intermediate Batches

- `intermediaria-32e70e48/`: first verified candidate, prior to cross-process durable queue.
- `intermediaria-fc711f6b/`: 13 verified attachments, with durable queue; replaced after island review discovered incomplete account/profile isolation. Dedicated manifest, hashes, and evidence reside in that directory.

No intermediate batch should replace the `b71471bc` candidate in final testing.

## Temporary Files and Storage

First Samsung compilation produced TPKs, but verification extraction failed due to disk space; that attempt was not accepted. On macOS, template-less mktemp ignored TMPDIR even in child bash. Explicit templates under TMPDIR resolved Android/ARM/TPK/Samsung verification (`fc711f6b`, with ARM adjustments integrated). Path regression on SSD passed.

Standalone TPK guard received `06d915f6`: grep -q exited early and SIGPIPE could mask personal files under pipefail. Fixture with 12k entries reproduced exit code 141; full consumption of listing passes clean case and rejects contaminated case. Current candidate already includes this tool; all Samsung ZIPs were also verified via full listing consumption.

Legacy builds from published/merged nv-166b worktree preserved in `/Volumes/ExternalSSD/nuvio-build-170-preservado`; ass-wasm remained in place to preserve references. Disposable 1.7.1/abrirrapido builds purged per handoff. Gradle, new builds, and temporaries reside on SSD. Cache of ten published 1.7.1 packages migrated with SHA256 to `/Volumes/ExternalSSD/nuvio-rel-171-cache`; original paths remain reachable via valid links.

## Remaining Gates

Technical package verification does not prove remote control, playback, GPU, HDR, or TV latency. Owner must validate C9/TCL/Samsung and test cases in LOCAL-VALIDATION.md. Island fix passed ASan/UBSan fixture and independent review; UX mockup proposals remain separate and were not implemented in these packages. Source/configuration/device dependent issues require targeted reproduction. No push/tag/publishing/comment authorized in this round.
