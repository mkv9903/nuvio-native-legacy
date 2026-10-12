# Fedora login regression

The first user check on Fedora 64-bit found that the Rust Test QR code did not
load and email login showed an incorrect-credentials error. The user confirmed
that both login methods work in the main-branch app on the same machine.

## Confirmed QR cause and correction

A live anonymous-service probe reproduced the QR failure. Anonymous
authentication returns a JSON object, while `start_tv_login_session` and
`poll_tv_login_session` return one-element arrays containing their records.
The first Rust implementation and its fixtures expected objects for both RPCs.

The Rust service now uses Serde to accept an object or a single object in an
array. It rejects empty arrays, multiple records, scalars and incomplete records.
The configured-page fallback also includes the encoded authorization code when
the server omits its web URL or supplies an empty one.

The live probe now completes anonymous authentication, QR creation and pending
polling successfully. It does not prove signed-in exchange on the user's account.
Automated tests cover both shapes, every poll outcome, malformed records and URL
fallback. The native screen test completes QR poll/exchange through the worker.

## Email investigation

The old C error mapper and initial Rust mapper called every HTTP 400 response
incorrect credentials. The user approved correcting this: use that message only
for a confirmed credentials error, preserving confirmation and rate-limit errors.
Other HTTP 400 failures report their status instead of blaming the password.

Diagnostics now record HTTP status and a fixed recognized category, without
printing request bodies, arbitrary service text, credentials or login URLs.
Startup also reports whether the compiled C and Rust account configurations
match; comparison exposes only a boolean. This can identify differing property
sources or Rust environment overrides without exposing keys.

The native regression now enters email and password through the retained C
keyboard completion/update path before dispatching the Rust action. Its request
assertions verify the complete input values using fake credentials. This passes,
but the cause of the user's real email failure remains unconfirmed until their
retest supplies the new diagnostics.

## Validation and retest

The updated backend passes 45 tests plus the native screen driver. The anonymous
live-service probe succeeds. Build/package checks accompany the pushed fix.
Retest Linux first, including both QR and email/password, before the first TV
runtime check. No user password is needed for diagnostics.

```bash
git pull --ff-only
NUVIO_LOG=/tmp/space.mkv.nuvio.rusttest.log bash tools/linux.sh
```

The optional anonymous probe uses the configured service, creates a short-lived
test code and saves no local account data. It reports status and reply shapes:

```bash
cd backend
cargo run --locked --example account_probe
```
