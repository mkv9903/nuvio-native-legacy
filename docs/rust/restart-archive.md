# Archived rewrite attempts

This branch is an inactive backup of the earlier helper-by-helper migration and
the standalone Rust UI experiment. The user rejected those approaches and chose
a clean restart from `main` on `codex/rust-backend`.

The unmerged standalone UI is saved here as unfinished work, not a release or a
TV prototype. Its synthetic screenshots are not a replacement for the cleaned
application's visual design. Do not continue this branch or cherry-pick its
implementation into the new rewrite.

The new direction keeps the C UI's appearance and remote navigation. Its models,
interfaces and data access may change to consume a Rust-owned backend. Complete
backend capabilities will be written idiomatically in Rust and tested in Rust
against the cases covered by the existing C implementation, with added coverage
where needed. Touched C UI components will be translated into English.

The first capability is login, sessions and profiles. Use versioned Rust JSON
with per-area migration of existing Rust Test data. Keep production-app data
separate. Local Linux testing precedes the first complete-flow TV handoff.
