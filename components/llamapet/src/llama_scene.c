/*
 * Escenario del corral: pasto, cerco, cordillera y objetos sueltos.
 */
#include "llama_scene.h"
#include "g2d.h"
#include "llama_pet.h"
#include <math.h>
#include <string.h>

#define COL_GRASS_A  g3d_rgb( 96, 168,  78)
#define COL_GRASS_B  g3d_rgb( 84, 154,  70)
#define COL_DIRT     g3d_rgb(168, 138,  94)
#define COL_WOOD     g3d_rgb(150, 106,  62)
#define COL_WOOD_D   g3d_rgb(122,  84,  48)
#define COL_MTN      g3d_rgb(108, 122, 156)
#define COL_MTN_SNOW g3d_rgb(238, 244, 252)
#define COL_BOWL     g3d_rgb(128,  98,  74)
#define COL_HAY      g3d_rgb(216, 188,  92)
#define COL_APPLE    g3d_rgb(210,  58,  52)
#define COL_LEAF     g3d_rgb( 74, 150,  62)
#define COL_BALL_A   g3d_rgb(236,  96, 120)
#define COL_BALL_B   g3d_rgb(250, 240, 200)
#define COL_POOP     g3d_rgb( 96,  70,  46)
#define COL_STONE    g3d_rgb(140, 140, 146)

static const float POOP_XY[LLAMA_POOP_SPOTS][2] = {
    { -1.35f,  0.85f }, { 1.45f, 0.35f }, { -0.75f, -1.55f },
    {  1.05f, -1.35f }, { 0.15f,  1.75f },
};

g3d_v3 llama_scene_poop_pos(int index)
{
    if (index < 0) index = 0;
    if (index >= LLAMA_POOP_SPOTS) index = LLAMA_POOP_SPOTS - 1;
    return g3d_v(POOP_XY[index][0], 0.f, POOP_XY[index][1]);
}

static void build_ground(g3d_mesh *m)
{
    const int   N = 5;          /* celdas por lado */
    const float S = 4.4f;       /* semilado del corral */
    const float step = (S * 2.f) / N;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float x0 = -S + i * step, x1 = x0 + step;
            float z0 = -S + j * step, z1 = z0 + step;
            g3d_color c = ((i + j) & 1) ? COL_GRASS_A : COL_GRASS_B;
            int a = g3d_mesh_vertex(m, g3d_v(x0, 0.f, z0));
            int b = g3d_mesh_vertex(m, g3d_v(x1, 0.f, z0));
            int c2 = g3d_mesh_vertex(m, g3d_v(x1, 0.f, z1));
            int d = g3d_mesh_vertex(m, g3d_v(x0, 0.f, z1));
            /* Antihorario visto desde arriba. */
            g3d_mesh_quad(m, a, d, c2, b, c);
        }
    }
    /* Pasto lejano para que el horizonte no muestre el vacio. */
    const float F = 26.f;
    struct { float x0, z0, x1, z1; } ring[4] = {
        { -F, -F,  F, -S }, { -F,  S,  F,  F },
        { -F, -S, -S,  S }, {  S, -S,  F,  S },
    };
    for (int i = 0; i < 4; i++) {
        int p0 = g3d_mesh_vertex(m, g3d_v(ring[i].x0, -0.01f, ring[i].z0));
        int p1 = g3d_mesh_vertex(m, g3d_v(ring[i].x1, -0.01f, ring[i].z0));
        int p2 = g3d_mesh_vertex(m, g3d_v(ring[i].x1, -0.01f, ring[i].z1));
        int p3 = g3d_mesh_vertex(m, g3d_v(ring[i].x0, -0.01f, ring[i].z1));
        g3d_mesh_quad(m, p0, p3, p2, p1, g3d_rgb(78, 146, 66));
    }

    /* Camino de tierra hacia el comedero. */
    int a = g3d_mesh_vertex(m, g3d_v(-0.55f, 0.01f, 1.10f));
    int b = g3d_mesh_vertex(m, g3d_v( 0.55f, 0.01f, 1.10f));
    int c = g3d_mesh_vertex(m, g3d_v( 0.75f, 0.01f, 2.60f));
    int d = g3d_mesh_vertex(m, g3d_v(-0.75f, 0.01f, 2.60f));
    g3d_mesh_quad(m, a, d, c, b, COL_DIRT);
}

static void build_fence(g3d_mesh *m)
{
    const float S = 4.3f;
    /* Postes en el fondo y en los laterales. */
    for (int i = -3; i <= 3; i++) {
        float x = i * 1.35f;
        g3d_mesh_box(m, g3d_v(x, 0.42f, -S), g3d_v(0.07f, 0.42f, 0.07f), COL_WOOD);
    }
    for (int i = -2; i <= 1; i++) {
        float z = -S + 1.2f + (i + 2) * 1.3f;
        g3d_mesh_box(m, g3d_v(-S, 0.40f, z), g3d_v(0.07f, 0.40f, 0.07f), COL_WOOD);
        g3d_mesh_box(m, g3d_v( S, 0.40f, z), g3d_v(0.07f, 0.40f, 0.07f), COL_WOOD);
    }
    /* Travesanos del fondo. */
    g3d_mesh_box(m, g3d_v(0.f, 0.62f, -S), g3d_v(4.1f, 0.05f, 0.04f), COL_WOOD_D);
    g3d_mesh_box(m, g3d_v(0.f, 0.34f, -S), g3d_v(4.1f, 0.05f, 0.04f), COL_WOOD_D);
}

static void build_mountains(g3d_mesh *m)
{
    struct { float x, z, w, h; } peaks[4] = {
        { -7.0f, -22.f, 9.0f, 5.4f },
        {  0.5f, -26.f, 11.f, 7.2f },
        {  7.5f, -21.f, 8.0f, 4.6f },
        {  3.0f, -18.f, 5.5f, 3.0f },
    };
    for (int i = 0; i < 4; i++) {
        g3d_mesh_pyramid(m, g3d_v(peaks[i].x, 0.f, peaks[i].z), peaks[i].w, peaks[i].w * 0.7f,
                         peaks[i].h, g3d_v(0.f, 0.f, 0.f), COL_MTN);
        /* Nieve en la cumbre. */
        g3d_mesh_pyramid(m, g3d_v(peaks[i].x, peaks[i].h * 0.62f, peaks[i].z),
                         peaks[i].w * 0.38f, peaks[i].w * 0.27f, peaks[i].h * 0.38f,
                         g3d_v(0.f, 0.f, 0.f), COL_MTN_SNOW);
    }
}

static void build_bowl(g3d_mesh *m)
{
    g3d_mesh_frustum(m, g3d_v(0.f, 0.f, 0.f), 0.44f, 0.44f, 0.56f, 0.56f, 0.16f,
                     g3d_v(0.f, 0.f, 0.f), COL_BOWL);
    g3d_mesh_box(m, g3d_v(0.f, 0.17f, 0.f), g3d_v(0.24f, 0.02f, 0.24f),
                 g3d_color_shade(COL_BOWL, 0.7f));
}

static void build_hay(g3d_mesh *m)
{
    g3d_mesh_box(m, g3d_v(0.f, 0.06f, 0.f), g3d_v(0.20f, 0.06f, 0.20f), COL_HAY);
    g3d_mesh_box(m, g3d_v(0.06f, 0.14f, -0.04f), g3d_v(0.12f, 0.05f, 0.12f),
                 g3d_color_shade(COL_HAY, 1.1f));
}

static void build_apple(g3d_mesh *m)
{
    g3d_mesh_box(m, g3d_v(0.f, 0.11f, 0.f), g3d_v(0.10f, 0.10f, 0.10f), COL_APPLE);
    g3d_mesh_box(m, g3d_v(0.f, 0.23f, 0.f), g3d_v(0.015f, 0.03f, 0.015f), COL_WOOD_D);
    g3d_mesh_box(m, g3d_v(0.06f, 0.25f, 0.f), g3d_v(0.05f, 0.01f, 0.03f), COL_LEAF);
}

static void build_ball(g3d_mesh *m)
{
    g3d_mesh_frustum(m, g3d_v(0.f, 0.f, 0.f), 0.16f, 0.16f, 0.22f, 0.22f, 0.11f,
                     g3d_v(0.f, 0.f, 0.f), COL_BALL_A);
    g3d_mesh_frustum(m, g3d_v(0.f, 0.11f, 0.f), 0.22f, 0.22f, 0.14f, 0.14f, 0.11f,
                     g3d_v(0.f, 0.f, 0.f), COL_BALL_B);
}

static void build_poop(g3d_mesh *m)
{
    g3d_mesh_pyramid(m, g3d_v(0.f, 0.f, 0.f), 0.16f, 0.16f, 0.13f,
                     g3d_v(0.f, 0.f, 0.f), COL_POOP);
    g3d_mesh_pyramid(m, g3d_v(0.10f, 0.f, 0.06f), 0.11f, 0.11f, 0.09f,
                     g3d_v(0.f, 0.f, 0.f), g3d_color_shade(COL_POOP, 0.85f));
}

static void build_grave(g3d_mesh *m)
{
    g3d_mesh_box(m, g3d_v(0.f, 0.30f, 0.f), g3d_v(0.26f, 0.30f, 0.07f), COL_STONE);
    g3d_mesh_box(m, g3d_v(0.f, 0.60f, 0.f), g3d_v(0.26f, 0.16f, 0.07f), COL_STONE);
    g3d_mesh_box(m, g3d_v(0.f, 0.52f, 0.08f), g3d_v(0.04f, 0.16f, 0.01f),
                 g3d_color_shade(COL_STONE, 0.6f));
    g3d_mesh_box(m, g3d_v(0.f, 0.60f, 0.08f), g3d_v(0.13f, 0.04f, 0.01f),
                 g3d_color_shade(COL_STONE, 0.6f));
}

static void build_rock(g3d_mesh *m)
{
    g3d_mesh_frustum(m, g3d_v(0.f, 0.f, 0.f), 0.42f, 0.38f, 0.24f, 0.22f, 0.34f,
                     g3d_v(0.03f, 0.f, 0.02f), COL_STONE);
}

bool llama_scene_init(llama_scene *s)
{
    memset(s, 0, sizeof(*s));
    bool ok = true;
    ok &= g3d_mesh_init(&s->ground, 160, 120);
    ok &= g3d_mesh_init(&s->fence, 160, 200);
    ok &= g3d_mesh_init(&s->mountains, 48, 40);
    ok &= g3d_mesh_init(&s->bowl, 24, 24);
    ok &= g3d_mesh_init(&s->hay, 24, 24);
    ok &= g3d_mesh_init(&s->apple, 32, 40);
    ok &= g3d_mesh_init(&s->ball, 24, 24);
    ok &= g3d_mesh_init(&s->poop, 16, 16);
    ok &= g3d_mesh_init(&s->grave, 40, 48);
    ok &= g3d_mesh_init(&s->rock, 16, 16);
    if (!ok) {
        llama_scene_free(s);
        return false;
    }
    build_ground(&s->ground);
    build_fence(&s->fence);
    build_mountains(&s->mountains);
    build_bowl(&s->bowl);
    build_hay(&s->hay);
    build_apple(&s->apple);
    build_ball(&s->ball);
    build_poop(&s->poop);
    build_grave(&s->grave);
    build_rock(&s->rock);
    s->ok = true;
    return true;
}

void llama_scene_free(llama_scene *s)
{
    g3d_mesh_free(&s->ground);
    g3d_mesh_free(&s->fence);
    g3d_mesh_free(&s->mountains);
    g3d_mesh_free(&s->bowl);
    g3d_mesh_free(&s->hay);
    g3d_mesh_free(&s->apple);
    g3d_mesh_free(&s->ball);
    g3d_mesh_free(&s->poop);
    g3d_mesh_free(&s->grave);
    g3d_mesh_free(&s->rock);
    s->ok = false;
}

void llama_scene_draw_mesh_at(g3d_target *t, const g3d_ctx *ctx, const g3d_mesh *m,
                              g3d_v3 pos, float yaw, float scale, float tint)
{
    g3d_mat4 mm = g3d_mat4_mul(g3d_mat4_translate(pos.x, pos.y, pos.z), g3d_mat4_rot_y(yaw));
    if (scale != 1.f) mm = g3d_mat4_mul(mm, g3d_mat4_scale(scale, scale, scale));
    g3d_draw_mesh(t, ctx, m, &mm, tint);
}

void llama_scene_draw_world(g3d_target *t, const g3d_ctx *ctx, const llama_scene *s, float night)
{
    const float tint = 1.f - night * 0.58f;
    g3d_mat4 id = g3d_mat4_identity();
    g3d_draw_mesh(t, ctx, &s->mountains, &id, tint * 0.95f);
    g3d_draw_mesh(t, ctx, &s->ground, &id, tint);
    g3d_draw_mesh(t, ctx, &s->fence, &id, tint);
}

void llama_scene_draw_poops(g3d_target *t, const g3d_ctx *ctx, const llama_scene *s,
                            int count, float night)
{
    const float tint = 1.f - night * 0.58f;
    for (int i = 0; i < count && i < LLAMA_POOP_SPOTS; i++) {
        llama_scene_draw_mesh_at(t, ctx, &s->poop, llama_scene_poop_pos(i),
                                 (float)i * 1.1f, 1.f, tint);
    }
}

void llama_scene_shadow(g3d_target *t, const g3d_ctx *ctx, g3d_v3 pos, float radius,
                        uint8_t alpha)
{
    float sx, sy, scale;
    if (!g3d_project(t, ctx, pos, &sx, &sy, &scale)) return;
    int rx = (int)(radius * scale);
    int ry = (int)(radius * scale * 0.42f);
    if (rx < 2) rx = 2;
    if (ry < 1) ry = 1;
    g2d_blend_ellipse(t, (int)sx, (int)sy, rx, ry, g3d_rgb(20, 40, 20), alpha);
}
