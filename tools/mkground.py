#!/usr/bin/env python3
"""Convierte las texturas cenitales del piso en el blob de baldosas de la placa.

Cada imagen (una por estacion, en el orden verano otonio invierno primavera) se
recorta al cuadrado central, se reescala al lado pedido y se vuelve repetible
fundiendo los bordes con su copia envuelta, porque los generadores prometen
"seamless" pero nunca lo cumplen del todo.

Uso:
    python3 tools/mkground.py verano.png otonio.png invierno.png primavera.png

Formato del blob:
    magic 'LGND'        4 bytes
    version 1           uint16
    count               uint16   estaciones
    size                uint16   lado de la baldosa (potencia de dos)
    span_q8             uint16   unidades de mundo por baldosa, en 8.8
    reserved            uint16 x3   (el encabezado mide 20 bytes en total)
    tabla               uint32 * count   offset de cada baldosa
    datos               uint16 * size*size   RGB565 por baldosa
"""
import argparse
import os
import struct

import numpy as np
from PIL import Image

HEAD_SIZE = 20


def to_rgb565(rgb):
    q = rgb.astype(np.uint16)
    return (((q[..., 0] & 0xF8) << 8) | ((q[..., 1] & 0xFC) << 3) |
            (q[..., 2] >> 3)).astype('<u2')


def make_tileable(a, border):
    """Funde cada borde con el opuesto para que el mosaico no muestre costura."""
    a = a.astype(np.float32)
    n = a.shape[0]
    w = ((np.arange(border) + 1) / (border + 1)).reshape(-1, 1, 1)
    # Vertical: las ultimas filas se mezclan con las primeras y viceversa.
    top = a[:border].copy()
    bot = a[n - border:].copy()
    a[:border] = top * (0.5 + 0.5 * w[::-1]) + bot[::-1] * (0.5 - 0.5 * w[::-1])
    a[n - border:] = bot * (0.5 + 0.5 * w) + top[::-1] * (0.5 - 0.5 * w)
    # Horizontal: igual sobre las columnas.
    wl = w.reshape(1, -1, 1)
    left = a[:, :border].copy()
    right = a[:, n - border:].copy()
    a[:, :border] = left * (0.5 + 0.5 * wl[:, ::-1]) + right[:, ::-1] * (0.5 - 0.5 * wl[:, ::-1])
    a[:, n - border:] = right * (0.5 + 0.5 * wl) + left[:, ::-1] * (0.5 - 0.5 * wl)
    return np.clip(a + 0.5, 0, 255).astype(np.uint8)


def build_tile(path, size, border):
    img = Image.open(path).convert('RGB')
    w, h = img.size
    side = min(w, h)
    img = img.crop(((w - side) // 2, (h - side) // 2,
                    (w + side) // 2, (h + side) // 2))
    img = img.resize((size, size), Image.LANCZOS)
    return make_tileable(np.asarray(img, dtype=np.uint8), border)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('images', nargs='+', help='una imagen por estacion')
    ap.add_argument('--out', default='assets/ground.bin')
    ap.add_argument('--preview', default='docs/img/ground.png')
    ap.add_argument('--size', type=int, default=256)
    ap.add_argument('--span', type=float, default=3.6,
                    help='unidades de mundo que cubre una baldosa')
    ap.add_argument('--border', type=int, default=24,
                    help='ancho de la mezcla de bordes, en pixeles')
    args = ap.parse_args()

    assert args.size & (args.size - 1) == 0, 'el lado tiene que ser potencia de dos'

    tiles = [build_tile(p, args.size, args.border) for p in args.images]

    head = struct.pack('<4sHHHHHHH', b'LGND', 1, len(tiles), args.size,
                       int(round(args.span * 256.0)), 0, 0, 0)
    assert len(head) == HEAD_SIZE, 'el encabezado tiene que medir GROUND_HEAD bytes'
    data_start = HEAD_SIZE + 4 * len(tiles)
    offsets, blobs, off = [], [], data_start
    for tl in tiles:
        data = to_rgb565(tl).tobytes()
        offsets.append(off)
        blobs.append(data)
        off += len(data)

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, 'wb') as f:
        f.write(head + b''.join(struct.pack('<I', o) for o in offsets) + b''.join(blobs))
    print('escrito %s: %d baldosas de %dx%d, %.0f KB'
          % (args.out, len(tiles), args.size, args.size, off / 1024))

    os.makedirs(os.path.dirname(args.preview), exist_ok=True)
    # La vista previa muestra cada baldosa en mosaico de 2x2 para ver la costura.
    sheets = [np.tile(tl, (2, 2, 1)) for tl in tiles]
    Image.fromarray(np.concatenate(sheets, axis=1)).save(args.preview)
    print('vista previa en %s' % args.preview)


if __name__ == '__main__':
    main()
