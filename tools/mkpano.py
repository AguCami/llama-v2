#!/usr/bin/env python3
"""Convierte las imagenes de paisaje en la tira panoramica que usa la placa.

Cada imagen (una por estacion) se recorta a una banda alrededor del horizonte,
se reescala y se espeja para que la vuelta de 360 grados quede sin costura.

El ancho de la tira sale del campo de vision: con 35,26 grados horizontales,
una vuelta completa son 360/35,26 = 10,2 pantallas.

Uso:
    python3 tools/mkpano.py verano.png otonio.png invierno.png primavera.png

Formato del blob:
    magic 'LPAN'        4 bytes
    version 1           uint16
    count               uint16   estaciones
    width, height       uint16 x2
    horizon             uint16   fila de la tira donde cae el horizonte
    reserved            uint16 x3   (el encabezado mide 20 bytes en total)
    tabla               uint32 * count   offset de cada tira
    datos               uint16 * width*height   RGB565 por tira
"""
import argparse
import math
import os
import struct

import numpy as np
from PIL import Image

SCREEN_W, SCREEN_H = 240, 284
FOVY = 0.72
CAM_DIST, CAM_HEIGHT, CAM_TARGET_Y = 4.55, 1.62, 0.88


def strip_width():
    hfov = 2.0 * math.atan(math.tan(FOVY * 0.5) * SCREEN_W / SCREEN_H)
    screens = 2.0 * math.pi / hfov
    return int(round(SCREEN_W * screens))


def screen_horizon():
    """Fila de la pantalla donde cae el horizonte con la camara del juego."""
    pitch = math.atan2(CAM_HEIGHT - CAM_TARGET_Y, CAM_DIST)   # cuanto mira abajo
    px_per_rad = SCREEN_H / FOVY
    return int(round(SCREEN_H * 0.5 - pitch * px_per_rad))


def to_rgb565(rgb):
    q = rgb.astype(np.uint16)
    return (((q[..., 0] & 0xF8) << 8) | ((q[..., 1] & 0xFC) << 3) |
            (q[..., 2] >> 3)).astype('<u2')


def build_strip(path, W, H, horizon_row, horizon_frac, band):
    img = Image.open(path).convert('RGB')
    sw, sh = img.size
    hy = horizon_frac * sh

    # Banda del original: `band` * (parte sobre el horizonte), y la parte de
    # abajo en la misma proporcion que la tira.
    up_src = band * hy
    down_src = up_src * (H - horizon_row) / max(horizon_row, 1)
    top = int(round(hy - up_src))
    bot = int(round(hy + down_src))
    top = max(top, 0)
    bot = min(bot, sh)
    crop = img.crop((0, top, sw, bot))

    half = W // 2
    crop = crop.resize((half, H), Image.LANCZOS)
    a = np.asarray(crop, dtype=np.uint8)
    return np.concatenate([a, a[:, ::-1]], axis=1)[:, :W]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('images', nargs='+', help='una imagen por estacion')
    ap.add_argument('--out', default='assets/pano.bin')
    ap.add_argument('--preview', default='docs/img/pano.png')
    ap.add_argument('--height', type=int, default=96)
    ap.add_argument('--horizon-row', type=int, default=80,
                    help='fila de la tira donde queda el horizonte')
    ap.add_argument('--horizon-frac', type=float, default=0.70,
                    help='altura relativa del horizonte en la imagen original')
    ap.add_argument('--band', type=float, default=0.5,
                    help='que fraccion del cielo original se usa (1 = todo)')
    args = ap.parse_args()

    W = strip_width()
    H = args.height
    print('tira: %d x %d, horizonte en la fila %d' % (W, H, args.horizon_row))
    print('horizonte de la pantalla del juego: fila %d' % screen_horizon())

    strips = [build_strip(p, W, H, args.horizon_row, args.horizon_frac, args.band)
              for p in args.images]

    head = struct.pack('<4sHHHHHHHH', b'LPAN', 1, len(strips), W, H,
                       args.horizon_row, 0, 0, 0)
    assert len(head) == 20, 'el encabezado tiene que medir PANO_HEAD bytes'
    table_size = 4 * len(strips)
    data_start = len(head) + table_size
    offsets, blobs, off = [], [], data_start
    for s in strips:
        data = to_rgb565(s).tobytes()
        offsets.append(off)
        blobs.append(data)
        off += len(data)

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, 'wb') as f:
        f.write(head + b''.join(struct.pack('<I', o) for o in offsets) + b''.join(blobs))
    print('escrito %s: %d tiras, %.0f KB' % (args.out, len(strips), off / 1024))

    os.makedirs(os.path.dirname(args.preview), exist_ok=True)
    sheet = np.concatenate(strips, axis=0)
    Image.fromarray(sheet).save(args.preview)
    print('vista previa en %s' % args.preview)


if __name__ == '__main__':
    main()
