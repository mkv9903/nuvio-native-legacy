# Android Preview Installed — 10/03/2026

Intermediate installation authorized by owner, executed by Luna chat `01a10412-0de7-7480-9bd5-55a436a37c6c`. Zero release, tag/push, or deploy.

| Evidence | Value |
|---|---|
| Frozen source | `38b0efb0dc9b6cc80ef0382d67b094764ace5d43` |
| Parent / tree | `e688599116043057f72a83fbee9087cac1ea973d` / `7893014b4f8dc7d44b420e022a2238815a2d4733` |
| Build copy | `/Volumes/ExternalSSD/nuvio-180-android-preview` |
| Local packaging commits | `098cd245` (version), `701d4113` (comment socialvis.h) |
| Device / package | Smart TV Pro, Android 14 / `space.nuvio.nativelegacy` |
| Installed version | 1.8.0 / 10800, reconfirmed by coordinator via dumpsys |
| Device ABI | armeabi-v7a, APK includes also arm64-v8a |
| Installation | `adb install -r`, without uninstall / clear-data |
| APK SHA-256 | `b30cec8c6fe5c35736448dc132bf2db7640fe199591c9252240d4964505a4de9` |

Coordinator rechecked hash of produced APK. Luna verified expected certificate, ABIs, and absence of personal files; recorded successful build/installation/launch and physical screenshots in `build/release-1.8.0/` of that copy. Zero explicit crashes identified in examined window; does not prove prolonged stability.

Includes: Glass + 1.7.4 merge, monthly Schedule, Apple layout logo→buttons→info without synopsis/cast row, AutoSync engine/documents without player wiring, personal Seekr / local quota, and reproduced Home trailer fixes. Excludes: WIP #158, #233, and StreamFit at time of snapshot. Compact synopsis and consistency across screens requested after this install remain pending; intermediate build does not validate U01–U04 from plan.

Remote control tests remain required: Apple/Modern menu, 90% Settings, fan/restart, episodes/blur/CW, trailers, and playback/Seekr. Install/launch/screenshot do not prove these flows or LG/Samsung.

Commit `701d4113` comments out an accidental loose declaration in Social header; copied to integration. Version files in integration remain 1.7.4: preview 1.8.0 adjustment does not alter public release.

[Closing Plan](PLAN.md). Packages and temporary photos reside on SSD copy; final source will receive dedicated manifest.

## Second Preview — Validated Source Restored

Delivered following renewed owner authorization. Local packaging commit `ab62f65c`, originated from frozen source `/Volumes/ExternalSSD/nuvio-180-execution-51fxfkxd` (`manifest.json`, 1,262 hashes verified by executor). Retains 1.8.0/10800 preview label; not a release publication.

Now includes #158/#233 fixes, visual detail restoration from 1.7.4 preserving fan/episodes, Home/Library, and memory/persistence/timeout diagnostics. StreamFit and AutoSync remain subject to integration limits in plan.

Initial Android link detected stale incremental object after copying sources with preserved timestamps. All sources touched and recompiled across both ABIs, without logic changes. Produced APK and `base.apk` extracted again from device share identical SHA-256: `eafe136e3b1950abce781af1ff3a6a0ce8fb06646956212a8956d89eca27f8ad`.

`adb install -r` passed; version 1.8.0/10800 and active process reconfirmed via root on Smart TV Pro `192.168.1.128:5555`. Inspected physical capture shows profile selector with existing profiles; zero clear-data or uninstall. Capture: `/Volumes/ExternalSSD/nuvio-180-android-preview/build/release-1.8.0/android-tv-capture.png`. Proves installation/startup, not visual approval of details or playback/sync/performance on hardware.

## Third Preview — Continuity, Ratings, and Progressive Home

Authorized by owner on 10/03. Source frozen at `/Volumes/ExternalSSD/nuvio-180-preview-ui-home-final` with 1,275 src/tests files and SHA-256 manifest. Luna recompiled both ABIs, installed via `adb install -r` preserving data, and extracted installed APK. Coordinator matched both files: SHA-256 `8d012314de5b73d60958dec7f06c1f6620abd7842550d8308f66fb46b03216d4`.

Includes Apple TV continuous veil, 1.7.4 ratings, separated cast/recommendations (TV/movie route, scroll past 7 items, and pointer), progressive initial Home publication with CW/social, local performance preference guards, Seekr Settings, and additive N01 API. Seekr OSD and StreamFit wiring started AFTER freeze and are not in this APK. Retains 1.8.0/10800 preview label, without public release.

Physical screenshot `/Volumes/ExternalSSD/nuvio-180-android-preview/build/release-1.8.0/android-tv-home-settled.png` displays Home with Continue Watching, Friends, and For You; coordinator inspected. SDL logs confirm launch. Spaced screenshots do not measure row startup/latency, nor validate playback or animated transitions; still requires comparable physical testing.

Local packaging commit: `6fa8e2d4d8abb9c8b28054d720e9729f8bd7ca2d`. Executor reported using `monkey -p … 1` for launch; therefore selector exit is not attributed to verified auto-resume. Subsequent launches must use explicit component with `am start`. Zero physical latency comparison in this round.
