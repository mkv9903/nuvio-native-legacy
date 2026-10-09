#!/usr/bin/env python3
"""Codepoints that embedded Inter (Regular, Medium, and Bold simultaneously)
can render, one hexadecimal value per line. Used by tests/limpa.sh to verify that
cleaned addon text contains only existing glyphs (#144). Reuses cmap reader from
tools/idiomas.py, which does not depend on fontTools.

    python3 tools/glifos.py [family-prefix]   # default: InterDisplay
"""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import idiomas  # noqa: E402

FONTS = pathlib.Path(__file__).resolve().parent.parent / "deploy" / "app" / "fonts"


def main():
    prefix = sys.argv[1] if len(sys.argv) > 1 else "InterDisplay"
    sets = [idiomas.cmap_ttf(f) for f in sorted(FONTS.glob(prefix + "*.ttf"))]
    if not sets:
        sys.exit("no font with prefix " + prefix)
    for cp in sorted(set.intersection(*sets)):
        print("%X" % cp)


if __name__ == "__main__":
    main()
