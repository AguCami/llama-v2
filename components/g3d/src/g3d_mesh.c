/*
 * Construccion de mallas low-poly a partir de primitivas.
 * Convencion de bobinado: antihorario visto desde afuera.
 */
#include "g3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool g3d_mesh_init(g3d_mesh *m, int max_v, int max_t)
{
    memset(m, 0, sizeof(*m));
    m->v = (g3d_v3 *)malloc(sizeof(g3d_v3) * (size_t)max_v);
    m->n = (g3d_v3 *)calloc((size_t)max_v, sizeof(g3d_v3));
    m->t = (g3d_tri *)malloc(sizeof(g3d_tri) * (size_t)max_t);
    if (!m->v || !m->n || !m->t) {
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
    free(m->n);
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
    m->n[m->nv] = g3d_v(0.f, 0.f, 0.f);
    return m->nv++;
}

void g3d_mesh_tri(g3d_mesh *m, int a, int b, int c, g3d_color col)
{
    if (m->nt >= m->cap_t) return;
    m->t[m->nt].a = (uint16_t)a;
    m->t[m->nt].b = (uint16_t)b;
    m->t[m->nt].c = (uint16_t)c;
    m->t[m->nt].color = col;
    m->t[m->nt].flags = 0;
    m->nt++;
}

void g3d_mesh_quad(g3d_mesh *m, int a, int b, int c, int d, g3d_color col)
{
    g3d_mesh_tri(m, a, b, c, col);
    g3d_mesh_tri(m, a, c, d, col);
}

void g3d_mesh_transform(g3d_mesh *m, g3d_mat4 xf)
{
    for (int i = 0; i < m->nv; i++) {
        m->v[i] = g3d_mat4_point(&xf, m->v[i]);
        m->n[i] = g3d_v3_norm(g3d_mat4_dir(&xf, m->n[i]));
    }
}

/* Aplica una transformacion solo a los vertices agregados desde `first_v`. */
void g3d_mesh_transform_from(g3d_mesh *m, int first_v, g3d_mat4 xf)
{
    for (int i = first_v; i < m->nv; i++) {
        m->v[i] = g3d_mat4_point(&xf, m->v[i]);
        float l = g3d_v3_len(m->n[i]);
        if (l > 1e-6f) m->n[i] = g3d_v3_norm(g3d_mat4_dir(&xf, m->n[i]));
    }
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

/* ------------------------------------------------- superficie de revolucion */

void g3d_mesh_revolve(g3d_mesh *m, const g3d_ring *rings, int nrings, int sides,
                      float a0, float a1, g3d_color col, unsigned caps)
{
    const float span  = a1 - a0;
    const bool  closed = (span >= 6.2f);
    /* Un arco puede tener un solo sector (asi se teje la manta celda a celda);
     * una superficie cerrada necesita al menos tres para no degenerar. */
    if (nrings < 2 || sides < 1 || (closed && sides < 3)) return;
    const int   ncols  = closed ? sides : sides + 1;
    const int   first_v = m->nv;
    const int   first_t = m->nt;

    if (m->nv + nrings * ncols > m->cap_v) return;

    for (int i = 0; i < nrings; i++) {
        const g3d_ring *r = &rings[i];
        for (int j = 0; j < ncols; j++) {
            float a = a0 + span * (float)j / (float)sides;
            g3d_mesh_vertex(m, g3d_v(r->cx + r->rx * cosf(a), r->y,
                                     r->cz + r->rz * sinf(a)));
        }
    }

    /* Caras laterales: A=(i,j) D=(i+1,j) C=(i+1,j+1) B=(i,j+1) es antihorario
     * visto desde afuera. */
    for (int i = 0; i + 1 < nrings; i++) {
        for (int j = 0; j < sides; j++) {
            int jn = (j + 1) % ncols;
            int A = first_v + i * ncols + j;
            int B = first_v + i * ncols + jn;
            int C = first_v + (i + 1) * ncols + jn;
            int D = first_v + (i + 1) * ncols + j;
            g3d_mesh_quad(m, A, D, C, B, col);
        }
    }
    const int side_t_end = m->nt;

    /* Normales por vertice acumulando las caras laterales de esta pieza. */
    for (int k = first_t; k < side_t_end; k++) {
        g3d_tri *t = &m->t[k];
        t->flags |= G3D_TRI_SMOOTH;
        g3d_v3 fn = g3d_v3_cross(g3d_v3_sub(m->v[t->b], m->v[t->a]),
                                 g3d_v3_sub(m->v[t->c], m->v[t->a]));
        m->n[t->a] = g3d_v3_add(m->n[t->a], fn);
        m->n[t->b] = g3d_v3_add(m->n[t->b], fn);
        m->n[t->c] = g3d_v3_add(m->n[t->c], fn);
    }
    /* En una pieza cerrada la primera y la ultima columna son la misma arista:
     * ya quedaron unidas por el modulo, asi que solo falta normalizar. */
    for (int i = first_v; i < m->nv; i++) m->n[i] = g3d_v3_norm(m->n[i]);

    /* Tapas planas (no participan del suavizado). */
    if (caps & G3D_CAP_LO) {
        int c = g3d_mesh_vertex(m, g3d_v(rings[0].cx, rings[0].y, rings[0].cz));
        for (int j = 0; j < sides; j++) {
            int jn = (j + 1) % ncols;
            g3d_mesh_tri(m, c, first_v + jn, first_v + j, col);
        }
    }
    if (caps & G3D_CAP_HI) {
        const g3d_ring *r = &rings[nrings - 1];
        int base = first_v + (nrings - 1) * ncols;
        int c = g3d_mesh_vertex(m, g3d_v(r->cx, r->y, r->cz));
        for (int j = 0; j < sides; j++) {
            int jn = (j + 1) % ncols;
            g3d_mesh_tri(m, c, base + j, base + jn, col);
        }
    }
}

void g3d_mesh_blob(g3d_mesh *m, g3d_v3 center, float rx, float ry, float rz,
                   int sides, int stacks, g3d_color col)
{
    if (stacks < 2) stacks = 2;
    if (stacks > 8) stacks = 8;
    g3d_ring rings[10];
    const int n = stacks + 1;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (float)(n - 1);          /* 0..1 */
        float phi = (t - 0.5f) * 3.14159265f;          /* -pi/2 .. pi/2 */
        rings[i].y  = center.y + sinf(phi) * ry;
        float k = cosf(phi);
        rings[i].rx = rx * k;
        rings[i].rz = rz * k;
        rings[i].cx = center.x;
        rings[i].cz = center.z;
    }
    /* Los polos degeneran a radio 0: no hacen falta tapas. */
    g3d_mesh_revolve(m, rings, n, sides, 0.f, 6.2831853f, col, 0);
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
