#!/usr/bin/env python3
"""Validate the Portuguese-key / English-value UI table.

Run with --cobertura to inspect embedded font coverage.
The cmap reader is also used by glifos.py and fonte-cjk.py.
"""
import re, sys, pathlib

RAIZ = pathlib.Path(__file__).resolve().parent.parent
SRC = RAIZ / "src"
FONTES = RAIZ / "deploy" / "app" / "fonts"
FONTE_CJK = "DroidSansFallback-Subset.ttf"
LIT = r'"((?:[^"\\]|\\.)*)"'
LINHA_MESTRA = re.compile(r'^\s*\{\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\},?\s*$')

SIMPLES = {"n": 10, "t": 9, "r": 13, "0": 0, '"': 34, "\\": 92, "'": 39}

def decodificar(s):
    """Literal C -> bytes, como o compilador decodifica (\\xNN, \\uNNNN, simples)."""
    out, i, n = bytearray(), 0, len(s)
    while i < n:
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8"); i += 1; continue
        i += 1
        c = s[i]
        if c == "x":
            j = i + 1
            while j < n and s[j] in "0123456789abcdefABCDEF": j += 1
            out.append(int(s[i + 1:j], 16) & 0xFF); i = j
        elif c == "u":
            out += chr(int(s[i + 1:i + 5], 16)).encode("utf-8"); i += 5
        elif c in SIMPLES:
            out.append(SIMPLES[c]); i += 1
        else:
            raise ValueError("escape desconhecido \\" + c)
    return bytes(out)

MARCADOR = re.compile(rb"%[-+ #0]*[0-9]*(?:\.[0-9]+)?(?:hh|h|ll|l|z|j|t)?[diouxXeEfgGcsp%]")
def marcadores(b):
    return [m for m in MARCADOR.findall(b) if m != b"%%"]

def cmap_ttf(caminho):
    """Codepoints com glifo no cmap (formatos 4 e 12) de um TTF."""
    import struct
    d = caminho.read_bytes()
    n = struct.unpack(">H", d[4:6])[0]
    off = None
    for i in range(n):
        tag, _cs, o, _ln = struct.unpack(">4sIII", d[12 + 16 * i:28 + 16 * i])
        if tag == b"cmap": off = o
    cps = set()
    nt = struct.unpack(">H", d[off + 2:off + 4])[0]
    for i in range(nt):
        _p, _e, so = struct.unpack(">HHI", d[off + 4 + 8 * i:off + 12 + 8 * i])
        s = off + so
        fmt = struct.unpack(">H", d[s:s + 2])[0]
        if fmt == 4:
            sc = struct.unpack(">H", d[s + 6:s + 8])[0] // 2
            fins = struct.unpack(">%dH" % sc, d[s + 14:s + 14 + 2 * sc])
            st = s + 16 + 2 * sc
            ini = struct.unpack(">%dH" % sc, d[st:st + 2 * sc])
            dl = st + 2 * sc
            delta = struct.unpack(">%dh" % sc, d[dl:dl + 2 * sc])
            ro = dl + 2 * sc
            rng = struct.unpack(">%dH" % sc, d[ro:ro + 2 * sc])
            for k in range(sc):
                for c in range(ini[k], fins[k] + 1):
                    if c == 0xFFFF: continue
                    if rng[k] == 0: g = (c + delta[k]) & 0xFFFF
                    else:
                        p = ro + 2 * k + rng[k] + 2 * (c - ini[k])
                        g = struct.unpack(">H", d[p:p + 2])[0]
                        if g: g = (g + delta[k]) & 0xFFFF
                    if g: cps.add(c)
        elif fmt == 12:
            ng = struct.unpack(">I", d[s + 12:s + 16])[0]
            for k in range(ng):
                a, b, g = struct.unpack(">III", d[s + 16 + 12 * k:s + 28 + 12 * k])
                if g: cps.update(range(a, b + 1))
    return cps

def ler_mestra():
    itens = []
    for n, linha in enumerate((SRC / "idioma_tab.h").read_text(encoding="utf-8").splitlines(), 1):
        if linha.lstrip().startswith("//") or not linha.strip():
            continue
        m = LINHA_MESTRA.match(linha)
        if not m:
            raise SystemExit("idioma_tab.h:%d: linha fora do formato { \"pt\", \"en\" }" % n)
        itens.append((n, m.group(1), m.group(2)))
    return itens


def conferir():
    mestra = ler_mestra()
    erros = []
    ant = None
    for n, pt, en in mestra:
        key = decodificar(pt)
        value = decodificar(en)
        if ant is not None and ant >= key:
            erros.append("idioma_tab.h:%d: keys are duplicated or unordered" % n)
        ant = key
        if not value.strip():
            erros.append("idioma_tab.h:%d: empty English value" % n)
        if marcadores(key) != marcadores(value):
            erros.append("idioma_tab.h:%d: printf markers differ" % n)
        if key.count(b"\n") != value.count(b"\n"):
            erros.append("idioma_tab.h:%d: newline counts differ" % n)
        value.decode("utf-8")
    return mestra, erros

def cobertura():
    values = set("".join(decodificar(en).decode("utf-8") for _, _, en in ler_mestra()))
    for font in sorted(FONTES.glob("*.ttf")):
        glyphs = cmap_ttf(font)
        missing = sum(ord(ch) not in glyphs for ch in values if ch >= " ")
        print("%s: %d English-table characters outside this font" % (font.name, missing))

if __name__ == "__main__":
    if sys.argv[1:] == ["--cobertura"]:
        cobertura()
        sys.exit(0)
    if sys.argv[1:]:
        sys.exit(__doc__)
    mestra, erros = conferir()
    if erros:
        sys.exit("\n".join(erros[:60]))
    print("idiomas: %d English entries passed" % len(mestra))
