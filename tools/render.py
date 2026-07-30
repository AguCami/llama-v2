#!/usr/bin/env python3
"""Rasterizador offline con textura, para convertir el GLB en sprites.

No corre en la placa: corre acá, sin limite de tiempo, sobre los 29.314
triangulos completos. La iluminacion imita la del motor del juego (ambiente
hemisferico + difusa) para que el sprite se integre con el corral 3D.
"""
import numpy as np


def look_at(eye, target, up=(0.0, 1.0, 0.0)):
    eye = np.asarray(eye, dtype=np.float64)
    target = np.asarray(target, dtype=np.float64)
    f = target - eye
    f /= np.linalg.norm(f)
    s = np.cross(f, np.asarray(up, dtype=np.float64))
    s /= np.linalg.norm(s)
    u = np.cross(s, f)
    m = np.eye(4)
    m[0, :3], m[1, :3], m[2, :3] = s, u, -f
    m[0, 3], m[1, 3], m[2, 3] = -s @ eye, -u @ eye, f @ eye
    return m


def perspective(fovy, aspect, znear, zfar):
    f = 1.0 / np.tan(fovy * 0.5)
    m = np.zeros((4, 4))
    m[0, 0] = f / aspect
    m[1, 1] = f
    m[2, 2] = (zfar + znear) / (znear - zfar)
    m[2, 3] = (2 * zfar * znear) / (znear - zfar)
    m[3, 2] = -1.0
    return m


class Renderer:
    """Rasteriza con z-buffer, UV con correccion de perspectiva y normales
    interpoladas. Devuelve color RGB y cobertura alfa."""

    def __init__(self, width, height, ss=3):
        self.w, self.h, self.ss = width, height, ss
        self.W, self.H = width * ss, height * ss

    def render(self, P, N, UV, F, tex, view_proj, light,
               ambient=0.56, diffuse=0.56, hemi=(0.70, 0.46)):
        W, H = self.W, self.H
        color = np.zeros((H, W, 3), dtype=np.float32)
        alpha = np.zeros((H, W), dtype=np.float32)
        depth = np.zeros((H, W), dtype=np.float32)      # guarda 1/w

        # --- proyeccion de todos los vertices de una sola vez ---
        ones = np.ones((len(P), 1))
        clip = np.hstack([P, ones]) @ view_proj.T
        w = clip[:, 3]
        visible = w > 1e-6
        invw = np.where(visible, 1.0 / np.where(visible, w, 1.0), 0.0)
        sx = (clip[:, 0] * invw * 0.5 + 0.5) * W
        sy = (0.5 - clip[:, 1] * invw * 0.5) * H

        # --- sombreado por vertice ---
        nl = np.linalg.norm(N, axis=1, keepdims=True)
        nn = N / np.where(nl < 1e-9, 1.0, nl)
        lam = np.clip(nn @ np.asarray(light, dtype=np.float64), 0.0, None)
        sky = 0.5 + 0.5 * nn[:, 1]
        shade = ambient * (hemi[0] + hemi[1] * sky) + diffuse * lam

        a, b, c = F[:, 0], F[:, 1], F[:, 2]
        ok = visible[a] & visible[b] & visible[c]
        # cara frontal: determinante negativo con Y invertido en pantalla
        area = ((sx[c] - sx[a]) * (sy[b] - sy[a]) -
                (sx[b] - sx[a]) * (sy[c] - sy[a]))
        keep = ok & (area > 1e-9)
        tris = np.flatnonzero(keep)

        th, tw = (tex.shape[0], tex.shape[1]) if tex is not None else (1, 1)
        texf = tex.astype(np.float32) / 255.0 if tex is not None else None

        for t in tris:
            i0, i1, i2 = F[t]
            x0, x1, x2 = sx[i0], sx[i1], sx[i2]
            y0, y1, y2 = sy[i0], sy[i1], sy[i2]
            minx = max(int(np.floor(min(x0, x1, x2))), 0)
            maxx = min(int(np.ceil(max(x0, x1, x2))), W - 1)
            miny = max(int(np.floor(min(y0, y1, y2))), 0)
            maxy = min(int(np.ceil(max(y0, y1, y2))), H - 1)
            if minx > maxx or miny > maxy:
                continue

            ar = ((x2 - x0) * (y1 - y0) - (x1 - x0) * (y2 - y0))
            if ar <= 0:
                continue
            inv_ar = 1.0 / ar

            xs = np.arange(minx, maxx + 1) + 0.5
            ys = np.arange(miny, maxy + 1) + 0.5
            px, py = np.meshgrid(xs, ys)

            e0 = (y2 - y1) * (px - x1) - (x2 - x1) * (py - y1)
            e1 = (y0 - y2) * (px - x2) - (x0 - x2) * (py - y2)
            e2 = (y1 - y0) * (px - x0) - (x1 - x0) * (py - y0)
            inside = (e0 >= 0) & (e1 >= 0) & (e2 >= 0)
            if not inside.any():
                continue

            w0 = e0[inside] * inv_ar
            w1 = e1[inside] * inv_ar
            w2 = e2[inside] * inv_ar

            iw = w0 * invw[i0] + w1 * invw[i1] + w2 * invw[i2]
            sub = depth[miny:maxy + 1, minx:maxx + 1]
            closer = iw > sub[inside]
            if not closer.any():
                continue

            rows, cols = np.nonzero(inside)
            rows, cols = rows[closer], cols[closer]
            w0, w1, w2, iw = w0[closer], w1[closer], w2[closer], iw[closer]

            # UV con correccion de perspectiva
            safe = np.where(np.abs(iw) < 1e-12, 1e-12, iw)
            u = (w0 * UV[i0, 0] * invw[i0] + w1 * UV[i1, 0] * invw[i1] +
                 w2 * UV[i2, 0] * invw[i2]) / safe
            v = (w0 * UV[i0, 1] * invw[i0] + w1 * UV[i1, 1] * invw[i1] +
                 w2 * UV[i2, 1] * invw[i2]) / safe

            if texf is not None:
                tx = np.clip((u % 1.0) * (tw - 1), 0, tw - 1).astype(np.int32)
                ty = np.clip((v % 1.0) * (th - 1), 0, th - 1).astype(np.int32)
                base = texf[ty, tx]
            else:
                base = np.full((len(w0), 3), 0.82, dtype=np.float32)

            sh = (w0 * shade[i0] + w1 * shade[i1] + w2 * shade[i2])[:, None]
            depth[miny + rows, minx + cols] = iw
            color[miny + rows, minx + cols] = np.clip(base * sh, 0.0, 1.0)
            alpha[miny + rows, minx + cols] = 1.0

        return self._downsample(color, alpha)

    def _downsample(self, color, alpha):
        s = self.ss
        h, w = self.h, self.w
        a = alpha.reshape(h, s, w, s).mean(axis=(1, 3))
        # promedio ponderado por cobertura, para que el borde no tire a negro
        cw = (color * alpha[..., None]).reshape(h, s, w, s, 3).sum(axis=(1, 3))
        tot = alpha.reshape(h, s, w, s).sum(axis=(1, 3))[..., None]
        c = np.divide(cw, tot, out=np.zeros_like(cw), where=tot > 0)
        return c, a


def rgb565(color, alpha, bg=(0.0, 0.0, 0.0)):
    """Convierte a RGB565 mezclando contra `bg` donde el alfa es parcial."""
    c = color * alpha[..., None] + np.asarray(bg, dtype=np.float32) * (1 - alpha[..., None])
    q = np.clip(c * 255.0, 0, 255).astype(np.uint16)
    return (((q[..., 0] & 0xF8) << 8) | ((q[..., 1] & 0xFC) << 3) | (q[..., 2] >> 3))
