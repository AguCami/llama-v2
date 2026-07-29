#!/usr/bin/env python3
"""Convierte los PPM del simulador a PNG (sin dependencias externas)."""
import sys
import os
import zlib
import struct


def read_ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    if not data.startswith(b"P6"):
        raise ValueError("no es un PPM binario: %s" % path)
    fields, pos = [], 2
    while len(fields) < 3:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b"#":
            while data[pos:pos + 1] not in (b"\n", b""):
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    pos += 1
    w, h, _maxval = fields
    return w, h, data[pos:pos + w * h * 3]


def write_png(path, w, h, rgb, scale=1):
    if scale > 1:
        big = bytearray()
        for y in range(h):
            row = rgb[y * w * 3:(y + 1) * w * 3]
            wide = bytearray()
            for x in range(w):
                px = row[x * 3:x * 3 + 3]
                wide += px * scale
            big += wide * scale
        rgb = bytes(big)
        w, h = w * scale, h * scale

    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3:(y + 1) * w * 3]

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else "build/shots"
    scale = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    paths = []
    if os.path.isdir(target):
        paths = [os.path.join(target, n) for n in sorted(os.listdir(target))
                 if n.endswith(".ppm")]
    else:
        paths = [target]
    for p in paths:
        w, h, rgb = read_ppm(p)
        out = p[:-4] + ".png"
        write_png(out, w, h, rgb, scale)
        print(out)


if __name__ == "__main__":
    main()
