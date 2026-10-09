# Settings: Artwork Without Repetition and Horizontal Controls

Native 1920×1080 captures on Mac, using local fixtures without account. Captured categories: all 11. Preserved samples: Languages, Live TV, and numeric editor with Glass. Does not represent validation on physical TV hardware.

- [Languages](idiomas.png)
- [Live TV](tv-ao-vivo.png)
- [Horizontal bar](barra-horizontal.png)
- [Complete map of 196 options](../../settings-visual-map.md)

## Checks Executed

- Native build of fixture `tests/ajustes_shot.c`, with all app modules except `main.c`.
- `tests/ajustes_ux_interacao.c`: passed. Includes horizontal editing, bounds, cancellation, reset, persistence, animation reversal, reduced motion, and icon map coverage.
- `tests/ajustes_ux_dados.c`: passed (search, defaults, scope, dependencies, and secrets).
- All icons in new map have local Lucide SVG and embedded PNG.
- Visual inspection of captures across solid surfaces and Glass; `git diff --check` reported zero errors.

Test binaries compiled directly; no release packaging scripts or prolonged benchmarks were run. Data test uses default temp directory; remaining binaries, fixtures, and captures produced in worktree.

Pending: physical TV / remote control testing, behavior and smoothness on LG/Samsung/Android targets, all UI scales, and full translations. DESIGN.md was absent from base; styling adhered to `.impeccable/design.json` rules and existing Glass components.

## Implementation Files

- `src/ajustes.c`
- `src/ajustes_ux_desenho.inc`
- `src/ajustes_ux_ilha.inc`
- `src/ajustes_ux_interacao.inc`
- `src/ajustes_ux_previa.inc`
- `src/ajustes_ux_v2.inc`
- `src/ajustes_ux_visual.inc`
- `tests/ajustes_ux_interacao.c`

Documentation: visual map, this log, and the three captures.
