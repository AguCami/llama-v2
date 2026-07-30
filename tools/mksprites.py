#!/usr/bin/env python3
"""Convierte assets/ref/llama.glb en la hoja de sprites que usa la placa.

Renderiza el modelo completo (29.314 triangulos + textura 2048x2048) desde N
angulos con la misma camara y la misma luz que el motor del juego, recorta cada
resultado a su caja util y escribe un blob binario que el firmware mapea
directamente desde la flash.

Uso:  python3 tools/mksprites.py [--angles 16] [--ss 3] [--height 1.70]

Formato del blob (todo little endian):

    magic   'LSPR'          4 bytes
    version 1               uint16
    angles                  uint16
    poses                   uint16
    reserved                uint16
    anchor_x, anchor_y      int16 x2   punto de anclaje en pantalla (origen del
                                       modelo proyectado) con el que se
                                       renderizo todo
    ref_scale               float32    alto en pixeles del modelo al renderizar
    --- tabla, angles*poses entradas de 12 bytes ---
    ox, oy                  int16 x2   esquina del recorte respecto del anclaje
    w, h                    uint16 x2  tamano del recorte
    offset                  uint32     desplazamiento de los datos en el blob
    --- datos por sprite ---
    color                   uint16 * w*h   RGB565
    alpha                   uint8  * ceil(w*h/2)  4 bits por pixel
"""
import argparse
import os
import struct
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from glb import Glb                      # noqa: E402
from render import Renderer, look_at, perspective   # noqa: E402

SCREEN_W, SCREEN_H = 240, 284
FOV = 0.72
CAM_DIST, CAM_HEIGHT, CAM_TARGET_Y = 4.55, 1.62, 0.88
LIGHT = np.array([-0.42, 0.80, 0.42])
LIGHT /= np.linalg.norm(LIGHT)


def rot_y(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    m = np.eye(4)
    m[0, 0], m[0, 2], m[2, 0], m[2, 2] = c, s, -s, c
    return m


def canonical(P, N, height):
    """Y arriba, patas en y=0, centrado en X/Z y escalado a `height` unidades."""
    P = P.copy()
    k = height / (P[:, 1].max() - P[:, 1].min())
    P *= k
    P[:, 1] -= P[:, 1].min()
    P[:, 0] -= (P[:, 0].max() + P[:, 0].min()) * 0.5
    P[:, 2] -= (P[:, 2].max() + P[:, 2].min()) * 0.5
    return P, N


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--glb', default='assets/ref/llama.glb')
    ap.add_argument('--out', default='assets/llama_sprites.bin')
    ap.add_argument('--preview', default='docs/img/sprites.png')
    ap.add_argument('--angles', type=int, default=16)
    ap.add_argument('--ss', type=int, default=3)
    ap.add_argument('--height', type=float, default=1.70)
    args = ap.parse_args()

    g = Glb(args.glb)
    P, N, UV, F = g.mesh()
    tex = g.texture()
    print('modelo: %d vertices, %d triangulos, textura %s' %
          (len(P), len(F), None if tex is None else '%dx%d' % tex.shape[:2]))

    P, N = canonical(P, N, args.height)

    view = look_at((0.0, CAM_HEIGHT, CAM_DIST), (0.0, CAM_TARGET_Y, 0.0))
    proj = perspective(FOV, SCREEN_W / SCREEN_H, 0.15, 60.0)
    view_proj = proj @ view

    # Punto de anclaje: el origen del modelo (entre las patas) en pantalla.
    o = view_proj @ np.array([0.0, 0.0, 0.0, 1.0])
    anchor_x = (o[0] / o[3] * 0.5 + 0.5) * SCREEN_W
    anchor_y = (0.5 - o[1] / o[3] * 0.5) * SCREEN_H
    print('anclaje en pantalla: (%.1f, %.1f)' % (anchor_x, anchor_y))

    rnd = Renderer(SCREEN_W, SCREEN_H, ss=args.ss)
    sprites = []
    t0 = time.time()
    for i in range(args.angles):
        deg = 360.0 * i / args.angles
        M = rot_y(deg)
        Pw = P @ M[:3, :3].T
        Nw = N @ M[:3, :3].T
        color, alpha = rnd.render(Pw, Nw, UV, F, tex, view_proj, LIGHT)

        ys, xs = np.nonzero(alpha > 0.004)
        if len(xs) == 0:
            raise RuntimeError('el angulo %d salio vacio' % i)
        x0, x1 = xs.min(), xs.max() + 1
        y0, y1 = ys.min(), ys.max() + 1
        sprites.append({
            'ox': int(x0 - round(anchor_x)),
            'oy': int(y0 - round(anchor_y)),
            'w': int(x1 - x0), 'h': int(y1 - y0),
            'color': color[y0:y1, x0:x1],
            'alpha': alpha[y0:y1, x0:x1],
        })
        print('  angulo %3d deg -> recorte %dx%d en (%+d,%+d)   [%.1fs]'
              % (deg, x1 - x0, y1 - y0, x0 - round(anchor_x), y0 - round(anchor_y),
                 time.time() - t0))

    write_blob(args.out, sprites, anchor_x, anchor_y, args.angles, args.height)
    write_preview(args.preview, sprites)


def to_rgb565(color):
    q = np.clip(color * 255.0 + 0.5, 0, 255).astype(np.uint16)
    return (((q[..., 0] & 0xF8) << 8) | ((q[..., 1] & 0xFC) << 3) |
            (q[..., 2] >> 3)).astype('<u2')


def write_blob(path, sprites, ax, ay, angles, height):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    table, blobs = [], []
    offset = 0
    for s in sprites:
        col = to_rgb565(s['color'])
        a4 = np.clip(np.round(s['alpha'] * 15.0), 0, 15).astype(np.uint8).reshape(-1)
        if a4.size & 1:
            a4 = np.append(a4, 0)
        packed = (a4[0::2] | (a4[1::2] << 4)).astype(np.uint8)
        data = col.tobytes() + packed.tobytes()
        table.append((s['ox'], s['oy'], s['w'], s['h'], offset))
        blobs.append(data)
        offset += len(data)

    head = struct.pack('<4sHHHHhhf', b'LSPR', 1, angles, 1, 0,
                       int(round(ax)), int(round(ay)), float(height))
    tbl = b''.join(struct.pack('<hhHHI', *e) for e in table)
    data_start = len(head) + len(tbl)
    tbl = b''.join(struct.pack('<hhHHI', e[0], e[1], e[2], e[3], e[4] + data_start)
                   for e in table)
    with open(path, 'wb') as f:
        f.write(head + tbl + b''.join(blobs))
    total = data_start + offset
    print('escrito %s: %d sprites, %.0f KB' % (path, len(sprites), total / 1024))


def write_preview(path, sprites):
    from PIL import Image
    os.makedirs(os.path.dirname(path), exist_ok=True)
    cols = 8
    rows = (len(sprites) + cols - 1) // cols
    cw = max(s['w'] for s in sprites) + 4
    ch = max(s['h'] for s in sprites) + 4
    sheet = np.full((rows * ch, cols * cw, 3), 0.90, dtype=np.float32)
    for i, s in enumerate(sprites):
        r, c = divmod(i, cols)
        y, x = r * ch + 2, c * cw + 2
        a = s['alpha'][..., None]
        sheet[y:y + s['h'], x:x + s['w']] = s['color'] * a + 0.90 * (1 - a)
    Image.fromarray((sheet * 255).astype(np.uint8)).save(path)
    print('vista previa en %s' % path)


if __name__ == '__main__':
    main()
