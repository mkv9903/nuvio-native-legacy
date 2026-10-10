#!/usr/bin/env python3
"""Regenerates deploy/app/fonts/DroidSansFallback-Subset.ttf, the embedded CJK font.

WHY IT EXISTS: text.c routes Japanese and Chinese to a FALLBACK font. The embedded font covers CJK titles and metadata on LG and Linux. See docs/fonts/README.md.

INPUT: Droid Sans Fallback 2.54 (Apache 2.0, Google 2006) shipped on LG TVs
in /usr/share/fonts/DroidSansFallback.ttf:
    scp root@<tv>:/usr/share/fonts/DroidSansFallback.ttf /tmp/
OUTPUT: ~7,900 glyphs, unhinted, ~1.2 MB:
  - ASCII, Latin-1, general punctuation, arrows, CJK punctuation, kana, fullwidth;
  - GB2312 level 1 and 2 partial (common simplified hanzi), JIS X 0208
    level 1 (everyday kanji), and common Big5 (traditional hanzi):
    most common characters in TMDB titles and synopses, preventing tofu glyphs
    when UI is in English and media is Korean, Chinese, or Japanese.

Usage:  python3 tools/fonte-cjk.py /tmp/DroidSansFallback.ttf
Requires fontTools (pip install fonttools).
"""
import pathlib, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import idiomas as I  # noqa: E402  (reuses table parser)

def char_range(a, b):
    return {chr(c) for c in range(a, b + 1)}

def decode_pairs(pairs1, pairs2, codec):
    out = set()
    for b1 in pairs1:
        for b2 in pairs2:
            try: out.add(bytes([b1, b2]).decode(codec))
            except UnicodeDecodeError: pass
    return out

def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    source = pathlib.Path(sys.argv[1])
    used = set()
    gb = decode_pairs(range(0xB0, 0xD8), range(0xA1, 0xFF), "gb2312")
    jis = decode_pairs(range(0x88, 0x99), range(0x40, 0xFD), "shift_jis")
    b5 = decode_pairs(range(0xA4, 0xC7), list(range(0x40, 0x7F)) + list(range(0xA1, 0xFF)), "big5")
    base = (char_range(0x20, 0x7E) | char_range(0xA0, 0xFF) | char_range(0x2010, 0x2027) | char_range(0x2190, 0x2193) |
            char_range(0x3000, 0x303F) | char_range(0x3040, 0x30FF) | char_range(0xFF00, 0xFFEF) | set("★▶✓•"))
    all_chars = used | base | gb | jis | b5
    text_file = pathlib.Path("/tmp/nuvio-cjk-caracteres.txt")
    text_file.write_text("".join(sorted(all_chars)), encoding="utf-8")
    dest = I.FONTES / I.FONTE_CJK
    subprocess.check_call([sys.executable, "-m", "fontTools.subset", str(source),
                           "--text-file=%s" % text_file, "--output-file=%s" % dest,
                           "--no-hinting", "--desubroutinize", "--layout-features=",
                           "--notdef-outline", "--drop-tables+=DSIG"])
    print("%s: %d characters requested, %d bytes" % (dest, len(all_chars), dest.stat().st_size))

if __name__ == "__main__":
    main()
