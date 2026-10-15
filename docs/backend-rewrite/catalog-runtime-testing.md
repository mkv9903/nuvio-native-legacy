# Integrated catalog test build

The native app now uses Rust for Home catalog HTTP requests, response parsing,
provider-document caching, complete title/row ownership and private scene JSON.
The C UI retains its layout, illustrations and remote navigation. C Home
assembly, metadata enrichment, source/subtitle searches, collection services,
general settings and native LG playback remain for their respective rewrites.
This is an incremental test build, not a completed backend rewrite.

Home requests use the complete Rust manifest catalog selected by its pinned
addon/catalog indices. Native display projections do not determine the request
URL or truncate provider configuration, catalog IDs or returned title values.
Each cycle carries a Rust context token. Scoped metadata and Continue Watching
callbacks cannot update the next profile, while ordinary Home refreshes do not
cancel valid metadata work. Metadata readers pin their addon snapshots, and
transient metadata caches are cleared when the displayed context changes.
Previous accounts/profiles, superseded cycles and late network replies cannot
publish through the guarded Home path.

Scene restore runs on one Rust worker and applies at a frame boundary. A changed
selection shows its private memory cache or an empty/loading Home immediately.
Shared primary addons still use the selected profile's private scene. A delayed
restore must match both the Rust view and the native catalog revision under the
publication lock. Confirmed empty catalog results disable the old-row fallback;
failed or malformed requests can reuse the last valid Rust provider document.

Only `rust-backend-v1` JSON from this rewrite is restored. The original binary
scene reader, writer, compression and format-compatibility code have been
removed. Its C tests were replaced by the Rust scene-cache/backend and native
catalog cases. There is no importer or original-file fallback. Unmigrated
capabilities retain their current implementation until their own rewrite.

## Local validation and manual checks

The Rust native catalog suite drives the actual C adapter/publication path. It
covers full request and title values, live capture/save, primary-addon profile
isolation, delayed replies, authoritative empty replies, malformed-response
fallback, row ownership across Continue Watching updates, stale cache views and
ignored original binary files. Native transition cases also retain chosen Home
and Details artwork after their source records are released. Settings row
previews now release their temporary title references.

Run the real account flow on Linux:

```sh
bash tools/linux.sh
```

To save the Linux log while testing:

```sh
NUVIO_LOG=/tmp/space.mkv.nuvio.rusttest.log bash tools/linux.sh
```

`--preview` intentionally uses the static illustration catalog and skips account
loading, so it does not test this capability. Linux media playback is stubbed;
the real video/audio path still requires a targeted webOS check.

1. Log in normally and confirm Home rows and the saved row order.
2. Switch between a profile with independent addons and one sharing primary
   addons. Confirm the previous profile's rows disappear while the target loads.
3. Open movie and series Details, view episodes and sources, then return Home.
   Confirm navigation, transitions, artwork and settings illustrations look the
   same as before.
4. Close and reopen with a selected profile. Confirm the private cached Home
   restores and the first network refresh does not show another profile's rows.

Build/test logs are recorded under `build/backend/catalog-tv-candidate-*`. No real
Nuvio account credentials are needed by the automated fixtures. Never commit
local service configuration, account data, cached provider documents or logs.

The candidate is checked with 196 Rust backend tests and 42 native integration
cases, plus the retained C window, Continue Watching, profile-preview and
catalog-lifetime regression groups. All 34 native catalog/title/capture cases
also pass with ASan/UBSan. Leak detection is disabled for process-lifetime app
globals; explicit final-owner assertions cover the transferred Rust records.
Linux compilation and a ten-second disposable normal-mode startup/shutdown pass.
The startup check reaches the login UI without using real account credentials.

## Packaged checkpoint

Application source `cc351086f55f7ec79a01cec535567187db8024a7` was built from
a clean tree on 2026-10-15 with Rust 1.85.1. The Linux checks above pass, as does
the webOS ARM release build with the retained C UI and native media libraries.
The runtime uses the soft-float ABI and imports glibc versions no newer than
2.12. Runtime/full/debug build IDs match:
`091bb7897485c48e03c3214e1cfe17b77cbcc035`.

The manual-test package is
`build/backend/webos/space.mkv.nuvio.rusttest_2.1.0_arm.ipk`, with the isolated
`space.mkv.nuvio.rusttest` identity and **Nuvio Rust Test** name. Package checks
verify the exact allowlisted runtime payload, executable permissions, and
exclusion of private/sample data and symbols. Its SHA-256 is
`4c7d556d45f271fdda15101dd1626cf79f1098a9a569d31a1a1198db55e70775`.
The matching executable, full binary, debug symbols and source/hash record are
under `build/backend/webos`; the previous matching package set is backed up
under `checkpoints/package-before-cc351086` there.

This package has not been installed or tested on a TV. In addition to the four
account/catalog checks above, test movie and series playback, resume, seeking,
audio/subtitle selection and returning to Home on webOS. The TV log is
`/tmp/space.mkv.nuvio.rusttest.log`. The remaining C backend capabilities still
need their own rewrites; this package validates the integrated catalog milestone.
