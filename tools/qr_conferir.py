#!/usr/bin/env python3
"""Verifies the QR generator by DECODING what it renders.

Comparing with another implementation is insufficient proof: two symbols may
differ module by module (different padding, different mask chosen) and both
remain valid. The only meaningful question is: can a real decoder read it?
Hence this verification decodes using OpenCV rather than comparing matrices.

Usage:
    cc tools/qr_despejo.c src/qr.c -o /tmp/qr_despejo -Isrc
    /tmp/qrvenv/bin/python tools/qr_conferir.py /tmp/qr_despejo
"""
import subprocess
import sys

import cv2
import numpy as np

CASES = [
    "https://nuvio.tv/tv-login?code=fa0010cad8b5d2f512e58646ab82ca6b",  # real case
    "https://nuvio.tv/tv-login?code=00000000000000000000000000000000",  # near-identical data
    "A",                    # smallest input
    "HELLO WORLD",
    "12345678901234567",
    "x" * 53,               # last fitting in version 3
    "y" * 54,               # first forcing version 4
    "z" * 78,               # last of version 4
    "w" * 106,              # version 5
    "k" * 134,              # version 6, module limit
]
# Beyond limit app must reject rather than drawing truncated.
CASES_SHOULD_FAIL = ["q" * 135, ""]


def matrix(binary, text):
    out = subprocess.run([binary, text], capture_output=True, text=True)
    lines = out.stdout.strip().splitlines()
    if not lines or lines[0].startswith("overflow") or lines[0].startswith("nao coube"):
        return None
    return [l.strip() for l in lines[1:]]


def decode_qr(m, scale=8, quiet=4):
    side = len(m)
    img = np.full((side + 2 * quiet, side + 2 * quiet), 255, dtype=np.uint8)
    img[quiet:quiet + side, quiet:quiet + side] = np.array(
        [[0 if c == "1" else 255 for c in line] for line in m], dtype=np.uint8
    )
    img = cv2.resize(img, None, fx=scale, fy=scale, interpolation=cv2.INTER_NEAREST)
    text, _, _ = cv2.QRCodeDetector().detectAndDecode(img)
    return text


def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else "/tmp/qr_despejo"
    failures = 0
    for text in CASES:
        m = matrix(binary, text)
        label = text if len(text) <= 34 else f"{text[:10]}…({len(text)}B)"
        if m is None:
            print(f"FAIL   {label}: rejected by app, but this fits")
            failures += 1
            continue
        read = decode_qr(m)
        if read == text:
            print(f"ok     {label}: {len(m)}x{len(m)} decoded identically")
        else:
            print(f"FAIL   {label}: {len(m)}x{len(m)} decoded as {read[:34]!r}")
            failures += 1
    for text in CASES_SHOULD_FAIL:
        if matrix(binary, text) is None:
            print(f"ok     ({len(text)}B): rejected as expected")
        else:
            print(f"FAIL   ({len(text)}B): should have been rejected")
            failures += 1
    print("\n" + ("ALL READABLE" if not failures else f"{failures} FAILURE(S)"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
