# First Rust-backed account flow check

Use branch `codex/rust-backend`. Install only the package whose identity is
`space.mkv.nuvio.rusttest` and title is **Nuvio Rust Test**. Its login/profile
screens retain the cleaned application's C renderer and remote navigation.
The production app remains a separate installation.

## Linux

Run `bash tools/linux.sh` for the normal account flow. `--preview` deliberately
skips account login and synchronization, so it cannot verify account rows.
Video playback uses the existing desktop stub.

## TV

Build with `bash tools/backend-webos.sh --ipk`, or use the supplied IPK from
`build/backend/webos`. This command packages the app without installing it.

1. On a fresh launch, check that the QR code appears, then complete phone login.
   Also check email/password login, including a wrong password and retry.
2. Check profile names/avatars, remote navigation, an unlocked profile and a
   PIN-protected profile. Verify that a wrong PIN stays in the picker and a
   correct PIN opens the chosen profile.
3. Restart the app. The Rust session should restore without signing in again;
   profile confirmation follows the existing picker rules for each launch.
4. Switch profiles, then log out and sign in again. If available, try another
   account to check that the old account's profile list is cleared.
5. Open Home, a title, its sources and one movie/episode as a compatibility smoke
   check for the retained C capabilities.

Share observations and `/tmp/space.mkv.nuvio.rusttest.log`. The Rust account
diagnostics use the `[account]` prefix and omit credentials.

Only this rewrite's `rust-backend-v1/account.json` supplies a saved Rust session.
C session files and files from discarded Rust attempts are not imported.
Unmigrated settings and other capabilities retain their C storage for now.

The known profile-switch Home catalog-row issue remains scheduled for the next
capability: Rust-owned addon/catalog loading with profile generations. This
account milestone should not be reported as fixing that issue.

Local checks cover fake-account HTTP, refresh/concurrency/outages, storage,
native snapshot ownership and the actual login/profile screens under Xvfb.
The first TV check must still establish the new HTTP/TLS runtime's behavior on
the TV itself; a successful ARM link or previous experiment's log cannot do so.
