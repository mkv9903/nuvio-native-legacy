# Profile catalog regression

This records the earlier profile-fix checkpoint. The current package, which
includes that correction, is documented in [Home coordination](home-coordination.md).

The Linux report identified two problems in the integrated catalog test build.
The account log showed seven Rust addons while the Home worker, after waiting
for account startup, still reported zero. It also showed the independent second
profile routed to the primary profile's addon list.

## Causes and corrections

The retained local "Use primary profile addons" option defaulted to enabled.
It overrode the sharing flag configured in the Nuvio account. The user approved
removing this override. The option, saved setting key, default, illustration
routing and accessor have been removed. Application profile selection and frame
updates now request account-defined sharing only. Independent profiles use
their own addons; profiles configured to share still use the primary list.
The settings addon count reads that same selected list.

The Home assembly thread retained its addon snapshot from before the account
wait. Catalog jobs were scoped correctly in Rust, but C's manifest enumeration
continued reading the thread's earlier empty or previous-profile snapshot.
At cycle start, after the wait, the adapter now activates Rust catalog state
and pins the exact addon snapshot belonging to the new Rust job. Manifest
indices, provider requests and publication therefore share one context.
Cancelled jobs cannot supply a replacement snapshot.

Only this rewrite's existing storage is used. There is no original-app file
import or compatibility reader. The removed local key is no longer recognized;
other settings continue using their own names.

## Regression coverage

Two Rust native tests fail against the previous C library: a worker waiting
before login reads zero rather than two arriving secondary-profile addons, and
the native account adapter routes an independent profile to profile one rather
than profile two. Three new Rust-driven native cases cover:

- A worker captures the empty pre-login list, the second profile is selected,
  its two addons arrive, and the actual C catalog worker loads its title.
- Independent profiles alternate with different addon counts, configured URLs
  and titles; each cycle replaces Home with the selected profile's records.
- The real Rust account wire decoder and native profile adapter select the
  secondary profile first, then alternate primary/independent/shared profiles.
  Account queries, native addon counts and sharing follow the server flags.

The retained C profile-settings and settings search/layout fixtures remove only
assertions for the deleted feature. Their runners now use the common Rust link
helper, as required by the current application sources. Replace these remaining
C settings cases with Rust when the settings backend is rewritten.

## Manual checks

Update `codex/rust-backend` and run normal Linux mode, without `--preview`:

```sh
NUVIO_LOG=/tmp/space.mkv.nuvio.rusttest.log bash tools/linux.sh
```

Choose the independent second profile first after a fresh login. Confirm its
Home catalogs load beyond Continue Watching. Switch to the primary profile,
back to the second, and repeat. Home rows and the Settings addon count should
match the selected profile. A profile configured in Nuvio to share primary
addons should still share them. Close/reopen to check private cached Home data.

For an isolated fresh-data check without deleting the normal test-app data:

```sh
NUVIO_DATA=/tmp/nuvio-rust-profile-check NUVIO_LOG=/tmp/space.mkv.nuvio.rusttest.log bash tools/linux.sh
```

Keep the same `NUVIO_DATA` value for a restart check. TV playback uses the retained
native media backend; the isolated Rust Test package can be used for targeted
profile/catalog and playback checks after local validation.

The corrected source passes 196 backend tests, 45 native integration tests,
Linux compilation and disposable normal-mode startup/shutdown. Both retained
settings fixture groups also pass. All 37 native catalog/title/capture cases
pass ASan/UBSan, including the new startup, independent-profile and native
account-routing cases. Leak detection is disabled for process-lifetime app
globals; explicit ownership assertions remain enabled.

## Verified test package

Application source `204511dd962f24f9a9cdeb4dc8004091db37d094` was built from a
clean tree on 2026-10-15. The webOS ARM build and isolated IPK pass package
payload/identity/permissions checks, the soft-float ABI check and imported glibc
maximum 2.12. The executable, full binary and debug symbols share build ID
`ab06913c19b196da96811dc914c8d3b7d6cbe80e`.

The package for that checkpoint is archived at
`build/backend/webos/checkpoints/package-before-78a6e57f/space.mkv.nuvio.rusttest_2.1.0_arm.ipk`
(**Nuvio Rust Test**, `space.mkv.nuvio.rusttest`). Its SHA-256 is
`dbbaa6a46430f00198a384e05e225d65d6193ec74f07a986ab44267cd5467c86`.
The matching runtime, symbols and source/hash record are in that same archive. The previous matching package set is preserved under
`checkpoints/package-before-204511dd` there. TV runtime testing is pending; no
package was installed or launched on a TV during these checks.
