/*
 * Construccion de mallas low-poly a partir de primitivas.
 * Convencion de bobinado: antihorario visto desde afuera.
 */
#include "g3d.h"
#include <stdlib.h>
#include <string.h>

bool g3d_mesh_init(g3d_mesh *m, int max_v, int max_t)
{
    memset(m, 0, sizeof(*m));
    m->v = (g3d_v3 *)malloc(sizeof(g3d_v3) * (size_t)max_v);
    m->t = (g3d_tri *)malloc(sizeof(g3d_tri) * (size_t)max_t);
    if (!m->v || !m->t) {
        g3d_mesh_free(m);
        return false;
    }
    m->cap_v = max_v;
    m->cap_t = max_t;
    return true;
}

void g3d_mesh_free(g3d_mesh *m)
{
    free(m->v);
    free(m->t);
    memset(m, 0, sizeof(*m));
}

void g3d_mesh_reset(g3d_mesh *m)
{
    m->nv = 0;
    m->nt = 0;
}

int g3d_mesh_vertex(g3d_mesh *m, g3d_v3 p)
{
    if (m->nv >= m->cap_v) return m->nv ? m->nv - 1 : 0;
    m->v[m->nv] = p;
    return m->nv++;
}

void g3d_mesh_tri(g3d_mesh *m, int a, int b, int c, g3d_color col)
{
    if (m->nt >= m->cap_t) return;
    m->t[m->nt].a = (uint16_t)a;
    m->t[m->nt].b = (uint16_t)b;
    m->t[m->nt].c = (uint16_t)c;
    m->t[m->nt].color = col;
    m->nt++;
}

void g3d_mesh_quad(g3d_mesh *m, int a, int b, int c, int d, g3d_color col)
{
    g3d_mesh_tri(m, a, b, c, col);
    g3d_mesh_tri(m, a, c, d, col);
}

void g3d_mesh_transform(g3d_mesh *m, g3d_mat4 xf)
{
    for (int i = 0; i < m->nv; i++) m->v[i] = g3d_mat4_point(&xf, m->v[i]);
}

void g3d_mesh_box(g3d_mesh *m, g3d_v3 c, g3d_v3 half, g3d_color col)
{
    g3d_mesh_frustum(m, g3d_v(c.x, c.y - half.y, c.z), half.x * 2.f, half.z * 2.f,
                     half.x * 2.f, half.z * 2.f, half.y * 2.f, g3d_v(0.f, 0.f, 0.f), col);
}

void g3d_mesh_frustum(g3d_mesh *m, g3d_v3 base, float bw, float bd,
                      float tw, float td, float height, g3d_v3 top_off, g3d_color col)
{
    const float bx = bw * 0.5f, bz = bd * 0.5f;
    const float tx = tw * 0.5f, tz = td * 0.5f;
    const float ty = base.y + height;
    const float ox = base.x + top_off.x, oz = base.z + top_off.z;
    const float oy = ty + top_off.y;

    /* Base (y menor), en orden: -x-z, +x-z, +x+z, -x+z */
    int b0 = g3d_mesh_vertex(m, g3d_v(base.x - bx, base.y, base.z - bz));
    int b1 = g3d_mesh_vertex(m, g3d_v(base.x + bx, base.y, base.z - bz));
    int b2 = g3d_mesh_vertex(m, g3d_v(base.x + bx, base.y, base.z + bz));
    int b3 = g3d_mesh_vertex(m, g3d_v(base.x - bx, base.y, base.z + bz));
    /* Tapa */
    int t0 = g3d_mesh_vertex(m, g3d_v(ox - tx, oy, oz - tz));
    int t1 = g3d_mesh_vertex(m, g3d_v(ox + tx, oy, oz - tz));
    int t2 = g3d_mesh_vertex(m, g3d_v(ox + tx, oy, oz + tz));
    int t3 = g3d_mesh_vertex(m, g3d_v(ox - tx, oy, oz + tz));

    g3d_mesh_quad(m, t0, t1, t2, t3, col);              /* arriba  (+Y) */
    g3d_mesh_quad(m, b3, b2, b1, b0, col);              /* abajo   (-Y) */
    g3d_mesh_quad(m, b3, b0, t0, t3, col);              /* izq     (-X) */
    g3d_mesh_quad(m, b1, b2, t2, t1, col);              /* der     (+X) */
    g3d_mesh_quad(m, b2, b3, t3, t2, col);              /* frente  (+Z) */
    g3d_mesh_quad(m, b0, b1, t1, t0, col);              /* atras   (-Z) */
}

void g3d_mesh_pyramid(g3d_mesh *m, g3d_v3 base, float bw, float bd, float height,
                      g3d_v3 apex_off, g3d_color col)
{
    const float bx = bw * 0.5f, bz = bd * 0.5f;
    int b0 = g3d_mesh_vertex(m, g3d_v(base.x - bx, base.y, base.z - bz));
    int b1 = g3d_mesh_vertex(m, g3d_v(base.x + bx, base.y, base.z - bz));
    int b2 = g3d_mesh_vertex(m, g3d_v(base.x + bx, base.y, base.z + bz));
    int b3 = g3d_mesh_vertex(m, g3d_v(base.x - bx, base.y, base.z + bz));
    int ap = g3d_mesh_vertex(m, g3d_v(base.x + apex_off.x, base.y + height + apex_off.y,
                                      base.z + apex_off.z));
    g3d_mesh_quad(m, b3, b2, b1, b0, col); /* base mirando hacia abajo */
    g3d_mesh_tri(m, b2, b3, ap, col);      /* +Z */
    g3d_mesh_tri(m, b1, b2, ap, col);      /* +X */
    g3d_mesh_tri(m, b0, b1, ap, col);      /* -Z */
    g3d_mesh_tri(m, b3, b0, ap, col);      /* -X */
}
