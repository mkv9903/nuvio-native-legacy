#!/usr/bin/env python3
"""Regenerates deploy/app/fonts/NotoNaskhArabic-Subset.ttf, the embedded Arabic font.

WHY IT EXISTS: text.c routes Arabic to a FALLBACK font. On LG, Android,
and Mac it comes from the system; on Samsung there is no Arabic font and text
rendered as missing glyph boxes (#253, #258). See docs/fonts/README.md.

REQUIREMENTS: Without HarfBuzz, src/bidi.c shapes Arabic by swapping each letter
to its PRESENTATION FORM (U+FE70-FEFF, and U+FB50-FDFF for Persian/Urdu). The
subset requires these two blocks, not just the base block U+0600-06FF.

INPUT: Noto Naskh Arabic Regular (OFL 1.1), from github.com/notofonts/arabic:
    fonts/NotoNaskhArabic/hinted/ttf/NotoNaskhArabic-Regular.ttf
OUTPUT: Without hinting and without layout tables (unused here).

Usage:  python3 tools/fonte-arabe.py /path/to/NotoNaskhArabic-Regular.ttf
Requires fontTools (pip install fonttools).
"""
import pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DEST = ROOT / "deploy" / "app" / "fonts" / "NotoNaskhArabic-Subset.ttf"
RANGES = ("U+0020-007F,U+00A0,U+0600-06FF,U+0750-077F,U+FB50-FDFF,U+FE70-FEFF,"
          "U+200C-200F,U+2010-2027")

def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    subprocess.check_call([sys.executable, "-m", "fontTools.subset", sys.argv[1],
                           "--unicodes=%s" % RANGES, "--output-file=%s" % DEST,
                           "--no-hinting", "--desubroutinize", "--layout-features=",
                           "--notdef-outline", "--drop-tables+=DSIG"])
    print("%s: %d bytes" % (DEST, DEST.stat().st_size))

if __name__ == "__main__":
    main()
