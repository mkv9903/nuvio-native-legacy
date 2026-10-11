# Settings illustrations

Category summaries and option previews use the generated drawings in
`src/ajustes_ux_cenas.inc`, backed by the drawing primitives in `src/ajcena.inc`.
`ajCenaGerada` draws a category when `op < 0` and an option otherwise. The
category IDs follow the order in `src/ajustes_ux_tela.inc` (`AJS_*`).

The drawings reflect the current setting, including pending values while
editing a row. They do not require bundled movie posters or sample catalogs.
The Settings background and row editor can use artwork from the user's own
catalog. On webOS, missing artwork uses neutral placeholders; desktop row
previews can use local development fixtures.

`ajCenaTroca` in `src/ajustes_ux_secoes.inc` handles category and option
transitions. Each generated category and option has its own transition key.
A normal transition fades over 0.22 seconds; held navigation waits for the
focus to settle. Initial displays, size changes, and reduced-animation mode
switch directly.

The Settings search preview uses the same generated drawings with a current
value overlay. Modal editors retain their contextual help and controls,
including the maximum-quality resolution bars and local image-memory panel.

The superseded photo-based category/submenu scenes and offscreen thumbnail
renderer have been removed. Tests in `tests/ajustes_ux_dados.c` retain the
generated-scene transition checks; `tests/ajustes_shot.sh` captures the active
Settings screens.
