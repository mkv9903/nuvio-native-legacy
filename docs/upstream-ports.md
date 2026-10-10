# Selected upstream reliability ports

These changes adapt 19 upstream commits after v2.0.2 to this LG webOS/Linux fork. Changes for removed platforms and features are omitted.

## LG font crash at startup

[`d423b051`](https://github.com/iqui27/nuvio-native-legacy/commit/d423b051f83b5d08da174b0ccedf9ab2f405950e).

## TV-guide search buffer overrun

[`8dd273f3`](https://github.com/iqui27/nuvio-native-legacy/commit/8dd273f3d05ee566d310cf2a867b13388b7072a7), [`3e3044eb`](https://github.com/iqui27/nuvio-native-legacy/commit/3e3044eb2d6a9986b763d9b9907a1246989a1392).

## Concurrent account token refresh

[`b324c7d0`](https://github.com/iqui27/nuvio-native-legacy/commit/b324c7d0e9b862a041208764a78d9e4ac644cb99).

## Other thread-safety fixes

[`25988de7`](https://github.com/iqui27/nuvio-native-legacy/commit/25988de7f5781cb5eeea199ded4b19bc1d0c0b0f), [`1a5d1670`](https://github.com/iqui27/nuvio-native-legacy/commit/1a5d1670327e69b94ecfc414bc0e1d38e65f661d), [`81c8f974`](https://github.com/iqui27/nuvio-native-legacy/commit/81c8f97446f23b9e335a633cf5b6032293c2d1e0), [`e332b1f8`](https://github.com/iqui27/nuvio-native-legacy/commit/e332b1f8ee100d5544b37fbea408370092287057), [`b3be79d7`](https://github.com/iqui27/nuvio-native-legacy/commit/b3be79d72503746b8665098e917271981edda37c), [`1c60454d`](https://github.com/iqui27/nuvio-native-legacy/commit/1c60454d63d562e0db6e3d799ec2b63f2babe699), [`0d33a0aa`](https://github.com/iqui27/nuvio-native-legacy/commit/0d33a0aa2dfe8d148d0649c9febb281d744ff29c).

## LG media-library lifetime and webOS version detection

[`65491e7b`](https://github.com/iqui27/nuvio-native-legacy/commit/65491e7b979da4f6b23e283fad1b58d3ce4ca682), [`00b4a937`](https://github.com/iqui27/nuvio-native-legacy/commit/00b4a937624a035ed65828c31cb63044fe47d881), [`ea72e607`](https://github.com/iqui27/nuvio-native-legacy/commit/ea72e6079906ae4125adffbb9e9f7aad04585840), [`79f23456`](https://github.com/iqui27/nuvio-native-legacy/commit/79f234560914d8ed40748df17658d3db3e55b18e), [`5303244f`](https://github.com/iqui27/nuvio-native-legacy/commit/5303244fc1bff15f63ca03499d62bf86e5968b3c), [`6c39c62b`](https://github.com/iqui27/nuvio-native-legacy/commit/6c39c62ba9e43056fc707c18b49de4cb1ab8c412), [`da6bece2`](https://github.com/iqui27/nuvio-native-legacy/commit/da6bece219dfb2580c28a5c6d9aa7c9b836bb08e), [`e6da2772`](https://github.com/iqui27/nuvio-native-legacy/commit/e6da2772a3efc68c8482edebcadba7c4bcdde8b4).

## Fork adaptations

- Session locking and token copies use the renamed `session.c` API and keep existing account integrations.
- Discovery receives the atomic worker-start guard; the newer upstream delayed-start scheduling feature is absent from this fork.
- Catalogue-order keys are copied under the lock into thread-local storage. Discovery retains separate copies when passing a batch to catalogue migration. The upstream two-bank implementation alone still produced torn names in the stress test.
- Early startup/current/previous/crash filenames use `space.mkv.nuvio.native`.
- Explore routing, screen, cache, settings, title action, guide entry, dedicated tests and exclusive assets/shader are removed. The line shader and other shared rendering code remain available to local diagnostics.
- Only confirmed obsolete Explore captions are removed. Other uncertain translation entries remain for the later English conversion.

## Validation

The targeted suite covers account login/refresh, watched progress, catalogue publication/order/cache, discovery, guide pagination, firmware-version parsing and DTS probes, navigation, settings, title pages, saved items and local diagnostics/crash storage. Concurrency and buffer checks also run with AddressSanitizer/UndefinedBehaviorSanitizer; leak detection is disabled because LeakSanitizer cannot run in this sandbox.

The English lookup-table and entry-format checks pass. The broader translation scanner still reports existing findings (38 missing-key findings and two format findings, versus 43 and two before this change), with no newly introduced findings. Full translation remains deferred.

Linux UI and LG ARM/IPK builds are checked. Binary inspection checks that FreeType symbols remain private, `pthread_create` remains exported for the DTS shim, and the version-test override/Explore code are absent. Software rendering verifies the retained sidebar destinations. LG firmware and playback behavior still need physical-TV validation.
