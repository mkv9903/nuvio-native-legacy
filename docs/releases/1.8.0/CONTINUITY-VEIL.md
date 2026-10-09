# Continuity of Apple Veil — Host GL

Stable renderer sources in integration checkout. Ownership restricted to
`src/detail.c:carFundo`, `src/gfx.c:GFX_JANELA` and its blend guard; fixtures
`tests/carrossel_shot.c/sh` and `tests/carrossel_continuidade.py`. No staging,
commit, push, or installation. Root layout/text/ratings/rows preserved.

The visual jump was the switch at `cartao=.002`: GFX_JANELA blended the veil onto solid
background, but GFX_DETALHE revealed ambient/Frost/blurred artwork via alpha. Now
the sheet converges to the page ground, selected base enters during
expansion, and the window converges to the same composition as details. The shader
also aligns scroll fade and the standalone trailer veil (including cinema).
Textureless placeholder and contained poster converge to the same endpoints.

There is no permanent crossfade between two pages or new fullscreen capture.
Ambient reuses the already baked pass. The settled page uses the previous
renderer: background-only Immersive 1 rect, Frost 6, blurred art 3; GFX_JANELA exits
upon completing expansion. Poster may use two artworks only during its
transition from cover to contained fit. Frost/blur expansion draws its
actual base beneath artwork; transient cost must be validated on TV.

## Proofs

Fixture captures background only at `cartao=1,.5,.1,.02,.0021,.0019,0`, using
real GL SDL/OpenGL on Mac and local artwork, without external provider. Compares
interior (8 px margin) in 8-bit RGB, tolerating grain/subpixel geometry.
`tests/carrossel_continuidade.py` uses standard Python library only and requires mean<=1,
p95<=3. All final cases below PASS:

| Case | Mean Before /255 | p95 Before | Mean After /255 | p95 After | Max After |
|---|---:|---:|---:|---:|---:|
| Immersive | 7.682 | 29 | .217 | 1 | 2 |
| Frost | 9.187 | 45 | .191 | 1 | 2 |
| Blurred Art | 4.117 | 16 | .219 | 1 | 3 |
| Art | .092 | 1 | .092 | 1 | 1 |
| Contained Poster | — | — | .274 | 1 | 22 (edges) |
| Scrolled Frost pg=.5 | — | — | .130 | 1 | 2 |
| Frost without Art | — | — | .033 | 0 | 1 |

Captures/pairs BMP+PNG: `/tmp/nuvio-car-bg-before-{immersive,frost,blur}` and
`/tmp/nuvio-car-bg-final-{immersive,frost,blur,plain,poster,scroll}`.
`c-limiar-4` and `c-limiar-5` are the two sides of threshold. Full sequence
`c-limiar-0` to `c-limiar-6`. Fixture prints branch/fill/rects per frame.

Reduced animations: complete normal path (open, switch title, open
page, back, close) PASS and finishes `detalhe aberto no fim: 0` in
`/tmp/nuvio-180-validation/car-bg-final-reduced.log`.
`git diff --check` PASS. Final mode logs in
`/tmp/nuvio-180-validation/car-bg-final-*.log`.

Missing artwork: corrected replay PASS in
`/tmp/nuvio-car-bg-final-missing-corrected`. First run was invalid:
fixture cleared IMDb but kept idxImdb and reconciler refound
a copy with artwork in catalog. Fixture only corrected; renderer unchanged.

Boundaries: host proof, no TV GPU-ms, no installation, no native trailer video.
Trailer composition was aligned in code; playback/device still
depends on platform proof. Root reviewed endpoint and blend guard with no
blocking findings and runs independent integrated capture.
