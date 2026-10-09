# Independent Review — Ratings and Cast/Recommendations

Integrated checkout, read-only of renderer / root layout. Edits authorized solely in `tests/detail_layout.c` for TMDB route regression, zero I/O. Root GL suite not repeated.

FINAL Rereview: root resolved all three items below; zero residual blockers observed during inspection. Movie/TV route PASS in `/tmp/nuvio-180-validation/detail-layout-tmdb-route.log`. Recommendations now render each `c` with `x - scrollSec` and `range[c, c+1]`, bound `EX_REL_MAX` and `revRel[EX_REL_MAX]`; culling on `c0` does not block remaining posters. Pointer height includes poster+caption and handler selects identical `c`. Root fixture now uses 10 items, column 8 and `scroll > 0` across both types; final GL belongs to root, not duplicated.

## Findings

1. P2 `src/detail.c:2195`: upon releasing `SEC_RELACIONADOS` on series, OK on an ID `tmdb:N` passed `movie` to `desc_pedir_titulo_tmdb`. The same numeric key belongs to distinct movie/TV namespaces. Root corrected to `ehSerie() ? "tv" : "movie"`. Regression added to layout fixture: stubs capture ID 42 + movie/TV type via real detail KEYDOWN/KEYUP, without network.

2. P2 `src/detail.c:1722`, `:5103`, `:6070`: count of `SEC_RELACIONADOS` accepts up to 12 items (`EX_REL_MAX`), but rendering loop iterated only the first 7 and ignored `scrollSec`. When focus moved to item 7+, no poster/focus was visible; outer loop culling could skip `c0` and block drawing of the entire row. Movie already had this fragility; change introduced it to series, whose legacy branch capped `relFoco` at 7. Solution: renderer iterates all items with coherent `scrollSec` and culling, or caps count (loses data). Important: `revRel` currently `[8]`; expand to `EX_REL_MAX` if rendering all. Existing fixture used only 3 cards and did not test this overflow case.

3. Pointer (pre-existing movie technical debt, not proven regression): `alturaAlvo(SEC_RELACIONADOS) == 0` and `desenhaRelacionados` does not register targets; Magic Remote cannot select or click these posters. When adjusting the new dedicated row, register the same actual renderer rects and use corresponding `c`; not cast avatar targets.

## Other Conclusions

- `git diff v1.7.4 -- src/notasui.c` contains only `notasui_media`. Renderer, measure/height, and heatmap/restoration are precisely those published in 1.7.4. Media API respects normalized sources and excludes aggregate via `nf_resumo`.
- Enum/focus and stacking place cast followed by recommendations across both types; subsequent groups use `conteudoSec` and document includes height of new row. `RelNaLista` does not highlight poster while cast holds focus.
- Arrival of extras recounts sections and recalculates stack per frame; zero new blockers observed in this flow. No guarantee of scroll stability upon real network arrival beyond available tests and inspection.
- Cast pointer preserves focus + OK / filmography handler.
- Original `NV_ROWS_ONLY` fixture proved D-pad forward/back navigation for 3 posters, movie + series. Root expanded to 10 items and column 8 in rereview; opaque TMDB route has separate passing regression. Physical clicks remain device limits.

Veil renderer source remains stable. Evidence is host/code; zero installation or physical LG/Samsung proof.
