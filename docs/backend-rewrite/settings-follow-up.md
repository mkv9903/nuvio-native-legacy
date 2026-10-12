# Settings rewrite notes

Settings behavior remains in C for this account/session/profile milestone.
The existing design and settings illustrations are retained.

The retained account-settings fixture in `tests/conta_ajustes.c` failed the same
four theme assertions on exact `main` and the backend branch. It repeatedly
read a temporary preferences file without initializing the application's data
directory. One-time appearance markers could not be persisted, so later reads
reset the theme instead of testing the intended preference.

The user approved a fixture-only correction: initialize an isolated temporary
data directory, write preferences there, and pass that directory consistently to
the existing settings reader. The complete fixture then passes. No product theme,
palette, settings or rendering behavior changed.

When settings move to Rust, replace this C fixture with Rust tests for:

- Default settings, fresh versioned JSON and restart persistence.
- Local/device/profile/account scope and precedence, including primary-addon
  overrides and profile-specific settings.
- Applying and merging account settings, partial/unknown values, boolean and
  enum mappings, and preservation of unknown account fields.
- Dynamic local themes versus account-supported themes, with stable typed
  values rather than positional theme indexes.
- Appearance initialization and any future schema changes exactly once, without
  conflating asset paths and data paths. Do not import legacy C preferences.
- The original settings screen layout, illustrations and remote navigation.

The data-directory fixture correction is temporary test infrastructure, not a
Rust settings implementation or a promise to support legacy settings files.
