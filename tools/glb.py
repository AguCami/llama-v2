#!/usr/bin/env python3
"""Lector minimo de glTF binario (.glb): posiciones, normales, UV, indices y textura.

Solo lo que necesita el pipeline de sprites; no pretende ser un cargador glTF
completo. Aplica las transformaciones de los nodos para que la malla salga en
coordenadas de mundo.
"""
import json
import struct
import io

import numpy as np

COMPONENT = {
    5120: ('b', 1), 5121: ('B', 1), 5122: ('h', 2),
    5123: ('H', 2), 5125: ('I', 4), 5126: ('f', 4),
}
NCOMP = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}


class Glb:
    def __init__(self, path):
        data = open(path, 'rb').read()
        if data[:4] != b'glTF':
            raise ValueError('no es un archivo .glb')
        off, self.js, self.bin = 12, None, None
        while off < len(data):
            clen, ctype = struct.unpack('<I4s', data[off:off + 8])
            off += 8
            chunk = data[off:off + clen]
            off += clen
            if ctype == b'JSON':
                self.js = json.loads(chunk)
            elif ctype[:3] == b'BIN':
                self.bin = chunk

    # ------------------------------------------------------------- accesores
    def accessor(self, index):
        acc = self.js['accessors'][index]
        bv = self.js['bufferViews'][acc['bufferView']]
        fmt, size = COMPONENT[acc['componentType']]
        n = NCOMP[acc['type']]
        stride = bv.get('byteStride') or size * n
        base = bv.get('byteOffset', 0) + acc.get('byteOffset', 0)
        count = acc['count']
        if stride == size * n:                      # empaquetado: lectura directa
            arr = np.frombuffer(self.bin, dtype=np.dtype(fmt), count=count * n,
                                offset=base).reshape(count, n)
        else:                                       # entrelazado: hay que saltar
            arr = np.empty((count, n), dtype=np.dtype(fmt))
            for k in range(count):
                arr[k] = struct.unpack_from('<' + fmt * n, self.bin, base + k * stride)
        return np.array(arr, dtype=np.float32 if fmt == 'f' else np.int64)

    # -------------------------------------------------------------- geometria
    def node_matrices(self):
        """Matriz de mundo por nodo, resolviendo la jerarquia de escenas."""
        nodes = self.js.get('nodes', [])
        out = [np.eye(4, dtype=np.float64)] * len(nodes)

        def local(nd):
            if 'matrix' in nd:                       # glTF la guarda por columnas
                return np.array(nd['matrix'], dtype=np.float64).reshape(4, 4).T
            m = np.eye(4)
            if 'scale' in nd:
                m = np.diag(list(nd['scale']) + [1.0]) @ m
            if 'rotation' in nd:
                x, y, z, w = nd['rotation']
                r = np.array([
                    [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0],
                    [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0],
                    [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0],
                    [0, 0, 0, 1],
                ])
                m = r @ m
            if 'translation' in nd:
                t = np.eye(4)
                t[:3, 3] = nd['translation']
                m = t @ m
            return m

        def walk(i, parent):
            world = parent @ local(nodes[i])
            out[i] = world
            for c in nodes[i].get('children', []):
                walk(c, world)

        roots = set(range(len(nodes)))
        for nd in nodes:
            for c in nd.get('children', []):
                roots.discard(c)
        for scene in self.js.get('scenes', []):
            for i in scene.get('nodes', []):
                roots.add(i)
        for i in sorted(roots):
            walk(i, np.eye(4))
        return out

    def mesh(self):
        """Devuelve (posiciones, normales, uv, triangulos) ya en mundo."""
        mats = self.node_matrices()
        P, N, T, F = [], [], [], []
        for ni, nd in enumerate(self.js.get('nodes', [])):
            if 'mesh' not in nd:
                continue
            world = mats[ni]
            rot = world[:3, :3]
            for prim in self.js['meshes'][nd['mesh']]['primitives']:
                attrs = prim['attributes']
                pos = self.accessor(attrs['POSITION']).astype(np.float64)
                pos = pos @ world[:3, :3].T + world[:3, 3]
                nrm = (self.accessor(attrs['NORMAL']).astype(np.float64) @ rot.T
                       if 'NORMAL' in attrs else np.zeros_like(pos))
                uv = (self.accessor(attrs['TEXCOORD_0']).astype(np.float64)
                      if 'TEXCOORD_0' in attrs else np.zeros((len(pos), 2)))
                idx = self.accessor(prim['indices']).reshape(-1).astype(np.int64)
                base = sum(len(p) for p in P)
                P.append(pos)
                N.append(nrm)
                T.append(uv)
                F.append(idx.reshape(-1, 3) + base)
        return (np.concatenate(P), np.concatenate(N),
                np.concatenate(T), np.concatenate(F))

    # --------------------------------------------------------------- textura
    def texture(self):
        """Primera imagen del archivo como arreglo RGB, o None."""
        from PIL import Image
        images = self.js.get('images', [])
        if not images:
            return None
        im = images[0]
        if 'bufferView' in im:
            bv = self.js['bufferViews'][im['bufferView']]
            start = bv.get('byteOffset', 0)
            raw = self.bin[start:start + bv['byteLength']]
            img = Image.open(io.BytesIO(raw))
        else:
            return None
        return np.asarray(img.convert('RGB'), dtype=np.uint8)
