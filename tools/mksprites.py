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


# --------------------------------------------------------------------- poses
#
# El sprite es una foto, asi que aplastarlo en 2D nunca va a parecer un cuello
# que baja: la cabeza se hunde en el cuerpo. Las poses se hacen deformando la
# geometria antes de renderizar, que es lo unico que da la silueta correcta.
#
# Medido sobre la malla ya canonica (altura 1,70; mira hacia +Z): el cuello es
# la columna angosta entre y=0,90 y y=1,10 alrededor de z=0,45, la cabeza va de
# ahi para arriba y el hocico llega a z=0,78. El vientre arranca en y=0,50.

NECK_Y, NECK_Z, NECK_LEN = 0.88, 0.45, 0.28
BELLY_Y = 0.50


def bend_neck(P, N, theta):
    """Baja cuello y cabeza girando alrededor de la base del cuello.

    `theta` en radianes: 0 deja la llama erguida, valores positivos bajan el
    hocico hacia adelante. El giro entra de a poco a lo largo del cuello (asi
    el hombro no se quiebra) y de la cabeza para arriba es rigido, que es como
    baja de verdad: el cuello barre, no se enrosca.
    """
    if abs(theta) < 1e-4:
        return P, N
    P, N = P.copy(), N.copy()
    h = P[:, 1] - NECK_Y
    up = h > 0.0
    if not up.any():
        return P, N

    u = np.clip(h[up] / NECK_LEN, 0.0, 1.0)
    phi = theta * (u * u * (3.0 - 2.0 * u))    # suavizado en la base
    c, sn = np.cos(phi), np.sin(phi)

    dy = h[up]
    dz = P[up, 2] - NECK_Z
    P[up, 2] = NECK_Z + dz * c + dy * sn
    P[up, 1] = NECK_Y - dz * sn + dy * c

    ny, nz = N[up, 1].copy(), N[up, 2].copy()
    N[up, 1] = ny * c - nz * sn
    N[up, 2] = ny * sn + nz * c
    return P, N


def cush(P, N, drop=0.78):
    """Postura echada: pliega las patas y hunde el cuerpo, como duerme de veras."""
    P = P.copy()
    legs = P[:, 1] < BELLY_Y
    P[legs, 1] *= (1.0 - drop)
    P[~legs, 1] -= BELLY_Y * drop
    return P, N


def pose_grazing(P, N, t):
    """Pastando: `t` va de 0 (erguida) a 1 (hocico a la altura del comedero).

    Con 2,0 rad el hocico queda en y=0,42 adelantado a z=0,68, justo sobre el
    comedero al que camina la llama.
    """
    return bend_neck(P, N, 2.0 * t)


def pose_resting(P, N):
    P, N = cush(P, N)
    return bend_neck(P, N, 0.30)          # cabeza apenas recogida


POSES = [
    ("parada",   lambda P, N: (P, N)),
    ("agachada", lambda P, N: pose_grazing(P, N, 0.5)),
    ("pastando", lambda P, N: pose_grazing(P, N, 1.0)),
    ("echada",   pose_resting),
]


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
    # La tabla va ordenada pose por pose: entrada = pose * angles + angulo.
    for pi, (pname, pfn) in enumerate(POSES):
        Pp, Np = pfn(P, N)
        for i in range(args.angles):
            deg = 360.0 * i / args.angles
            R = rot_y(deg)
            Pw = Pp @ R[:3, :3].T
            Nw = Np @ R[:3, :3].T
            color, alpha = rnd.render(Pw, Nw, UV, F, tex, view_proj, LIGHT)

            ys, xs = np.nonzero(alpha > 0.004)
            if len(xs) == 0:
                raise RuntimeError('%s / angulo %d salio vacio' % (pname, i))
            x0, x1 = xs.min(), xs.max() + 1
            y0, y1 = ys.min(), ys.max() + 1
            sprites.append({
                'ox': int(x0 - round(anchor_x)),
                'oy': int(y0 - round(anchor_y)),
                'w': int(x1 - x0), 'h': int(y1 - y0),
                'color': color[y0:y1, x0:x1],
                'alpha': alpha[y0:y1, x0:x1],
            })
        print('  pose %-9s lista (%d angulos)   [%.0fs]'
              % (pname, args.angles, time.time() - t0))

    write_blob(args.out, sprites, anchor_x, anchor_y, args.angles, len(POSES),
               args.height)
    write_preview(args.preview, sprites, args.angles)


def to_rgb565(color):
    q = np.clip(color * 255.0 + 0.5, 0, 255).astype(np.uint16)
    return (((q[..., 0] & 0xF8) << 8) | ((q[..., 1] & 0xFC) << 3) |
            (q[..., 2] >> 3)).astype('<u2')


def write_blob(path, sprites, ax, ay, angles, poses, height):
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

    head = struct.pack('<4sHHHHhhf', b'LSPR', 1, angles, poses, 0,
                       int(round(ax)), int(round(ay)), float(height))
    tbl = b''.join(struct.pack('<hhHHI', *e) for e in table)
    data_start = len(head) + len(tbl)
    tbl = b''.join(struct.pack('<hhHHI', e[0], e[1], e[2], e[3], e[4] + data_start)
                   for e in table)
    with open(path, 'wb') as f:
        f.write(head + tbl + b''.join(blobs))
    total = data_start + offset
    print('escrito %s: %d sprites, %.0f KB' % (path, len(sprites), total / 1024))


def write_preview(path, sprites, cols=8):
    from PIL import Image
    os.makedirs(os.path.dirname(path), exist_ok=True)
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
