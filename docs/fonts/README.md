# Fonts Distributed with Nuvio

Inter Display remains the interface default. Additional bundled families are Montserrat, Roboto, and Atkinson Hyperlegible Next. Interface font selection and subtitle font selection operate independently: the former resides in Appearance → Interface font; the latter is in the player's subtitle styling panel. ASS subtitles rendered via libass preserve fonts specified in the stream file.

Montserrat provides a geometric aesthetic alternative. It is not Netflix Sans and is not presented as an official Netflix typeface.

Additional font files are static, upstream releases in Regular (400), Medium (500), and Bold (700) weights. No synthetic weights were generated. SIL OFL 1.1 licenses accompany each family in `deploy/app/fonts/*-OFL.txt`.

## Provenance

- [JulietaUla/Montserrat](https://github.com/JulietaUla/Montserrat/tree/555facfb2a18c72c3c0380f0d9c0f060453a9058) — commit `555facfb2a18c72c3c0380f0d9c0f060453a9058`.
- [googlefonts/atkinson-hyperlegible-next](https://github.com/googlefonts/atkinson-hyperlegible-next/tree/7925f50f649b3813257faf2f4c0b381011f434f1) — commit `7925f50f649b3813257faf2f4c0b381011f434f1`.
- [Roboto v3.016](https://github.com/googlefonts/roboto-3-classic/releases/tag/v3.016) — `hinted/static/` files from official release.
- [Atkinson Hyperlegible, Braille Institute](https://www.brailleinstitute.org/freefont/) — official font page.

## Integrity

| File | Bytes | SHA-256 |
|---|---:|---|
| `AtkinsonHyperlegibleNext-Bold.ttf` | 66792 | `994414047df66bb4998d01c1cb1eeb4a2ddc4622d1aa56bbb8adbeca7645b041` |
| `AtkinsonHyperlegibleNext-Medium.ttf` | 67080 | `dd50b08b3c560846097d23baaaf6a97ffa20dd077115d23c59df68083b9ea05e` |
| `AtkinsonHyperlegibleNext-Regular.ttf` | 65068 | `88ed5c31a71584c7772963b02d04bef1eb7e3d2e9c8b9cb204339b1f82cf432c` |
| `Montserrat-Bold.ttf` | 454864 | `bc6e854971cea46b463be6f9eef4d9cd52f51cfc1fc0dd90c9d3e6483dc0ec61` |
| `Montserrat-Medium.ttf` | 447320 | `dae47428bb041f9716604e0e07b5b0c8585b3bdd8183362f75c69fe7bb3cfaf4` |
| `Montserrat-Regular.ttf` | 445928 | `3e8abe50c44c82e2242e97d1ec8c0d385c4890cdc50447bcdb8605c81a38cfb2` |
| `Roboto-Bold.ttf` | 465944 | `44f13e570a8b3fc42240abfcb3d72fe298f3dd6a25db0c78d0c2495dbd0e9121` |
| `Roboto-Medium.ttf` | 464100 | `80ce163ec5ecd91a883aa20e80d31dc74971b55cf658dd7c5e8d21f4e9fcb417` |
| `Roboto-Regular.ttf` | 463712 | `9f300202f482ad59f8b13bc3131a295744d14fd530fa3765a0a11ec87264d203` |

Total across all nine additional files: 2,940,808 bytes.

## Packaging

Build pipelines include `deploy/app/fonts` in native packages and WebAssembly preload assets. License files must accompany font files. Inclusion in repository does not demonstrate physical TV installation or behavior.

## Embedded CJK Font (Japanese and Chinese)

Inter lacks kana and hanzi glyphs. `text.c` routes the line to a fallback font when any character is missing; on LG and Mac fallback comes from system fonts, whereas Samsung WASM environments provide no system fonts. Therefore, the package includes `DroidSansFallback-Subset.ttf`.

- Origin: Droid Sans Fallback 2.54, copyright Google 2006, Apache License 2.0 (`DroidSansFallback-LICENSE.txt`). Extracted from `/usr/share/fonts/DroidSansFallback.ttf` on an LG C9.
- Modified: subset of 7,943 characters (ASCII, Latin-1, punctuation, kana, GB2312 Level 1, JIS X 0208 Level 1, common Big5, and all characters in ja/zhcn/zhtw tables), unhinted. A subset restricted to UI text alone would be 0.34 MB; this package is 1.2 MB because TMDB Chinese/Japanese titles and synopses also display when the UI is in another language.
- Regeneration: `python3 tools/fonte-cjk.py /tmp/DroidSansFallback.ttf` (requires fontTools).
- Consumer: CJK fallback only, following system fonts (`LG_Display_JP`, `DroidSansFallback`, `LG_Display-Regular`, `LG_Display_HK-Regular`; on Mac Hiragino/STHeiti). `tools/idiomas.py` verifies all characters in ja/zh tables are present.
- Visual inspection on Mac: `NUVIO_SEM_RESERVA_DE_SISTEMA=1 bash tests/idioma_shot.sh /tmp/x 27,28,29`.

| File | Bytes | SHA-256 |
|---|---:|---|
| `DroidSansFallback-Subset.ttf` | 1226652 | `7f4ff9c7ad1d4dd471e2f24ab27bc48b09d9dec0329a22b524f6664062eb7bc2` |

## Embedded Arabic Font

Samsung firmware lacks Arabic fonts, resulting in tofu blocks (#253, #258). The package includes `NotoNaskhArabic-Subset.ttf`, last in `ESC_ARABE` fallback list in `text.c`, after system fonts (LG, Android, Mac).

- Origin: Noto Naskh Arabic Regular 2.021, The Noto Project Authors, SIL OFL 1.1 (`NotoNaskhArabic-OFL.txt`), from github.com/notofonts/arabic.
- Modified: subset covering ASCII, U+0600–06FF, U+0750–077F, and presentation forms U+FB50–FDFF and U+FE70–FEFF, unhinted without OpenType layout tables. Presentation forms are mandatory: `src/bidi.c` performs letter joining by swapping individual glyph codepoints, without HarfBuzz.
- Regeneration: `python3 tools/fonte-arabe.py /path/to/NotoNaskhArabic-Regular.ttf` (requires fontTools).
- Verification on Mac: `bash tests/text_familias.sh` (opens renderer with `NUVIO_SEM_RESERVA_DE_SISTEMA=1`).

| File | Bytes | SHA-256 |
|---|---:|---|
| `NotoNaskhArabic-Subset.ttf` | 90372 | `a43af24c9307d838d89593fc86342ab024973ffa0d7f71bac76849afe2ce5675` |
