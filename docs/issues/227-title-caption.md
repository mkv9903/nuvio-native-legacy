# Localized title beside artwork (#227)

The detail page always shows the current catalog's textual title above the logo. The catalog's existing localization updates remain visible after opening; artwork keeps its independent session identity. No OCR, locale guessing, or additional metadata request is introduced.

The complete title wraps without a line limit. Its measured height is reserved above the logo, which shrinks when needed to keep the caption above the agenda, resume rows and action buttons. The logo is skipped if no drawable height remains. Titles without a logo use the same textual block.

Validation: `tests/detail_remonta.sh`, `tests/heroidentidade.sh`, and `NUVIO_SHOT_TITULO=1 bash tests/detail_eps_shot.sh <output-prefix>`. The last command uses production rendering with an offline synthetic catalog, verifies live title replacement, unrestricted wrapping and viewport bounds, and captures loaded-logo and no-logo variants. Different names and logo artwork in those fixtures intentionally exercise the localization mismatch. It is a Mac GL fixture, not physical-TV validation. The loading path uses the same unconditional caption branch but is not independently timed in the fixture; combined agenda/resume layout is anchored to the existing shared `yEstado` calculation.


## Adjustment After Android TV Testing

The user confirmed Discord working and requested textual title solely as language fallback. Detail view hides the name when the actively displayed logo has confirmed TMDB language matching configured language; artwork with unknown, foreign, missing, or in-flight loading language retains the readable text name. Language metadata is tied to the exact URL preserved in the catalog, preventing a new logo's language from being mistakenly attributed to a previous snapshot. The old catalog cache will be invalidated due to CatItem struct size change.

In movies, director moves to the technical metadata line beside country, eliminating previous dead space and bringing action buttons closer to the synopsis. In Dynamic layout (Apple TV), expanding the card to fullscreen now shifts text and logo to page margin on step one; the second arrow press opens the scrollable body. Back navigation restores position inside the card.

### Fix After Second TV Verification

Unknown language does not imply foreign: loaded logos without known language tag do not receive duplicate textual titles. During asset loading, no provisional title is shown that would disappear post-load; genuine absence or image failure permits the textual title. The director/writer line was also moved in series. In fullscreen Apple TV layout, the ends of the button row do not change titles or exit the page from the left; Back returns to carousel, where arrows can switch titles again. Dedicated test covers known/unknown language, URL divergence, image loading/missing, and navigation in both states.
