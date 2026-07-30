/*
 * Escenario del corral: pasto, cerco, cordillera y objetos sueltos.
 */
#include "llama_scene.h"
#include "g2d.h"
#include "llama_pet.h"
#include <math.h>
#include <string.h>

#define COL_GRASS_A  g3d_rgb( 96, 168,  78)
#define COL_GRASS_B  g3d_rgb( 90, 160,  74)
#define COL_GRASS_C  g3d_rgb( 84, 152,  70)
#define COL_DIRT     g3d_rgb(168, 138,  94)
#define COL_WOOD     g3d_rgb(150, 106,  62)
#define COL_WOOD_D   g3d_rgb(122,  84,  48)
#define COL_MTN      g3d_rgb(108, 122, 156)
#define COL_MTN_FAR  g3d_rgb(126, 140, 172)
#define COL_TUFT     g3d_rgb( 74, 148,  62)
#define COL_TUFT_D   g3d_rgb( 62, 130,  54)
#define COL_MTN_SNOW g3d_rgb(238, 244, 252)
#define COL_BOWL     g3d_rgb(128,  98,  74)
#define COL_HAY      g3d_rgb(216, 188,  92)
#define COL_APPLE    g3d_rgb(210,  58,  52)
#define COL_LEAF     g3d_rgb( 74, 150,  62)
#define COL_BALL_A   g3d_rgb(236,  96, 120)
#define COL_BALL_B   g3d_rgb(250, 240, 200)
#define COL_POOP     g3d_rgb( 96,  70,  46)
#define COL_STONE    g3d_rgb(140, 140, 146)
#define COL_CLOUD    g3d_rgb(252, 252, 255)
#define COL_CLOUD_D  g3d_rgb(226, 233, 244)

/* Paleta del piso por estacion: tres verdes/tonos para el damero suave, el
 * color del pasto lejano, el del camino y el de las matas. */
typedef struct {
    g3d_color a, b, c, far, dirt, tuft, tuft_d;
} season_palette;

/* Version macro de g3d_rgb: hace falta una expresion constante para poder
 * inicializar el arreglo estatico. */
#define C565(r, g, b) \
    ((g3d_color)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static const season_palette PALETTE[SEASON_COUNT] = {
    /* verano */
    { C565( 96,168, 78), C565( 90,160, 74), C565( 84,152, 70),
      C565( 74,140, 64), C565(168,138, 94),
      C565( 74,148, 62), C565( 62,130, 54) },
    /* otonio */
    { C565(186,148, 76), C565(176,138, 68), C565(164,126, 62),
      C565(150,114, 58), C565(158,120, 80),
      C565(158,116, 52), C565(136, 98, 46) },
    /* invierno */
    { C565(232,238,246), C565(222,230,242), C565(212,222,238),
      C565(202,214,234), C565(196,198,204),
      C565(180,196,214), C565(160,178,200) },
    /* primavera */
    { C565(112,186, 84), C565(104,176, 78), C565( 96,166, 74),
      C565( 86,154, 68), C565(172,144,100),
      C565( 88,168, 66), C565(112,180, 74) },
};

static const season_palette *g_pal = &PALETTE[SEASON_VERANO];

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
    const int   N = 7;          /* celdas por lado */
    const float S = 6.4f;       /* semilado del corral */
    const float step = (S * 2.f) / N;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float x0 = -S + i * step, x1 = x0 + step;
            float z0 = -S + j * step, z1 = z0 + step;
            g3d_color c = ((i * 3 + j * 5) % 3 == 0) ? g_pal->a
                        : (((i + j) & 1) ? g_pal->b : g_pal->c);
            int a = g3d_mesh_vertex(m, g3d_v(x0, 0.f, z0));
            int b = g3d_mesh_vertex(m, g3d_v(x1, 0.f, z0));
            int c2 = g3d_mesh_vertex(m, g3d_v(x1, 0.f, z1));
            int d = g3d_mesh_vertex(m, g3d_v(x0, 0.f, z1));
            /* Antihorario visto desde arriba. */
            g3d_mesh_quad(m, a, d, c2, b, c);
        }
    }
    /* Pasto lejano en anillos concentricos: al ser caras chicas, la niebla
     * degrada parejo en vez de cortarse de golpe. */
    const float RING_R[5] = { 6.4f, 9.0f, 13.f, 19.f, 34.f };
    const g3d_color RING_C[4] = {
        g3d_color_shade(g_pal->far, 1.10f), g3d_color_shade(g_pal->far, 1.04f),
        g_pal->far, g3d_color_shade(g_pal->far, 0.96f),
    };
    for (int r = 0; r < 4; r++) {
        const float r0 = RING_R[r], r1 = RING_R[r + 1];
        for (int seg = 0; seg < 12; seg++) {
            float a0 = seg * 0.5235988f, a1 = (seg + 1) * 0.5235988f;
            /* Cuadrado inscripto: usamos el radio como semilado proyectado. */
            float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
            int p0 = g3d_mesh_vertex(m, g3d_v(c0 * r0, -0.01f, s0 * r0));
            int p1 = g3d_mesh_vertex(m, g3d_v(c1 * r0, -0.01f, s1 * r0));
            int p2 = g3d_mesh_vertex(m, g3d_v(c1 * r1, -0.02f, s1 * r1));
            int p3 = g3d_mesh_vertex(m, g3d_v(c0 * r1, -0.02f, s0 * r1));
            g3d_color c = RING_C[r];
            c = g3d_color_lerp(g_pal->far, c, 0.f);
            if ((seg + r) % 3 == 0) c = g3d_color_shade(c, 1.06f);
            g3d_mesh_quad(m, p0, p3, p2, p1, c);
        }
    }

    /* Camino de tierra hacia el comedero. */
    int a = g3d_mesh_vertex(m, g3d_v(-0.55f, 0.01f, 1.10f));
    int b = g3d_mesh_vertex(m, g3d_v( 0.55f, 0.01f, 1.10f));
    int c = g3d_mesh_vertex(m, g3d_v( 0.75f, 0.01f, 2.60f));
    int d = g3d_mesh_vertex(m, g3d_v(-0.75f, 0.01f, 2.60f));
    g3d_mesh_quad(m, a, d, c, b, g_pal->dirt);
}

static void build_fence(g3d_mesh *m)
{
    /* Cerco completo: enmarca el corral se mire para donde se mire. */
    const float S = 6.3f;
    for (int side = 0; side < 4; side++) {
        const bool along_x = (side < 2);
        const float fixed  = (side & 1) ? S : -S;
        for (int i = -3; i <= 3; i++) {
            float t = i * 2.05f;
            g3d_v3 c = along_x ? g3d_v(t, 0.44f, fixed) : g3d_v(fixed, 0.44f, t);
            g3d_mesh_box(m, c, g3d_v(0.075f, 0.44f, 0.075f), COL_WOOD);
            /* Puntita mas clara arriba del poste. */
            g3d_mesh_box(m, g3d_v(c.x, 0.90f, c.z), g3d_v(0.085f, 0.035f, 0.085f),
                         g3d_color_shade(COL_WOOD, 1.15f));
        }
        for (int r = 0; r < 2; r++) {
            float y = r ? 0.66f : 0.38f;
            g3d_v3 half = along_x ? g3d_v(6.25f, 0.05f, 0.045f)
                                  : g3d_v(0.045f, 0.05f, 6.25f);
            g3d_v3 c = along_x ? g3d_v(0.f, y, fixed) : g3d_v(fixed, y, 0.f);
            g3d_mesh_box(m, c, half, COL_WOOD_D);
        }
    }
}

static void build_mountains(g3d_mesh *m)
{
    /* Cordillera alrededor del corral: siempre hay fondo, se mire para donde
     * se mire. La niebla de distancia las funde con el horizonte. */
    const float R = 23.f;
    for (int i = 0; i < 9; i++) {
        float a = (float)i * 0.698f + 0.25f;             /* ~40 grados entre picos */
        float rr = R + ((i * 7) % 5) * 1.6f;
        float x = sinf(a) * rr;
        float z = cosf(a) * rr;
        float h = 4.2f + (float)((i * 13) % 7) * 0.75f;
        float w = 8.f + (float)((i * 5) % 4) * 1.8f;
        g3d_color c = ((i & 1) ? COL_MTN : COL_MTN_FAR);
        g3d_ring cone[4] = {
            { -0.5f,        w * 0.50f, w * 0.38f, x, z },
            { h * 0.42f,    w * 0.30f, w * 0.23f, x, z },
            { h * 0.74f,    w * 0.15f, w * 0.11f, x, z },
            { h,            w * 0.01f, w * 0.01f, x, z },
        };
        g3d_mesh_revolve(m, cone, 4, 6, 0.f, 6.2831853f, c, 0);
        g3d_ring snow[3] = {
            { h * 0.68f, w * 0.175f, w * 0.135f, x, z },
            { h * 0.86f, w * 0.085f, w * 0.065f, x, z },
            { h * 1.01f, w * 0.01f,  w * 0.01f,  x, z },
        };
        g3d_mesh_revolve(m, snow, 3, 6, 0.f, 6.2831853f, COL_MTN_SNOW, 0);
    }
}

/* Matas de pasto sueltas: le sacan al piso el aire de tablero de ajedrez. */
static void build_tufts(g3d_mesh *m)
{
    static const float SPOT[16][2] = {
        { -4.6f, -2.6f }, { 3.8f, -4.0f }, { -2.9f, 3.9f }, { 4.7f, 2.1f },
        { -5.2f, 0.9f },  { 1.7f, 4.9f },  { -1.3f, -4.8f }, { 5.2f, -1.5f },
        { -3.9f, -4.6f }, { 0.5f, -5.4f }, { 5.4f, 4.3f },  { -4.8f, 4.7f },
        { -5.6f, -4.4f }, { 4.4f, -2.4f }, { -4.2f, -0.6f }, { 3.2f, 3.6f },
    };
    for (int i = 0; i < 16; i++) {
        float x = SPOT[i][0], z = SPOT[i][1];
        float h = 0.18f + (float)((i * 7) % 4) * 0.05f;
        g3d_mesh_pyramid(m, g3d_v(x, 0.f, z), 0.16f, 0.14f, h,
                         g3d_v(0.03f, 0.f, 0.02f), g_pal->tuft);
        g3d_mesh_pyramid(m, g3d_v(x + 0.11f, 0.f, z + 0.07f), 0.12f, 0.11f, h * 0.75f,
                         g3d_v(-0.03f, 0.f, 0.02f), g_pal->tuft_d);
    }
}

/* Nubes de verdad, no elipses 2D: se mueven con la camara y dan profundidad. */
static void build_clouds(g3d_mesh *m)
{
    static const float C[4][4] = {   /* x, y, z, escala */
        { -6.0f, 7.2f, -12.0f, 1.35f },
        {  5.5f, 8.4f, -15.0f, 1.70f },
        { 10.0f, 6.6f,   4.0f, 1.15f },
        { -9.0f, 7.8f,   7.0f, 1.45f },
    };
    for (int i = 0; i < 4; i++) {
        float k = C[i][3];
        g3d_v3 c = g3d_v(C[i][0], C[i][1], C[i][2]);
        g3d_mesh_blob(m, c, 2.2f * k, 0.85f * k, 1.6f * k, 6, 2, COL_CLOUD);
        g3d_mesh_blob(m, g3d_v(c.x + 1.6f * k, c.y - 0.25f * k, c.z + 0.4f * k),
                      1.5f * k, 0.62f * k, 1.2f * k, 5, 2, COL_CLOUD);
        g3d_mesh_blob(m, g3d_v(c.x - 1.7f * k, c.y - 0.30f * k, c.z - 0.3f * k),
                      1.3f * k, 0.55f * k, 1.1f * k, 5, 2, COL_CLOUD_D);
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
    ok &= g3d_mesh_init(&s->ground, 480, 360);
    ok &= g3d_mesh_init(&s->fence, 560, 700);
    ok &= g3d_mesh_init(&s->mountains, 480, 620);
    ok &= g3d_mesh_init(&s->tufts, 200, 220);
    ok &= g3d_mesh_init(&s->clouds, 260, 260);
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
    build_tufts(&s->tufts);
    build_clouds(&s->clouds);
    build_bowl(&s->bowl);
    build_hay(&s->hay);
    build_apple(&s->apple);
    build_ball(&s->ball);
    build_poop(&s->poop);
    build_grave(&s->grave);
    build_rock(&s->rock);
    s->season = SEASON_VERANO;
    s->ok = true;
    return true;
}

void llama_scene_set_season(llama_scene *s, llama_season season)
{
    if (!s->ok) return;
    if (season < 0 || season >= SEASON_COUNT) season = SEASON_VERANO;
    if (s->season == season) return;
    s->season = season;
    g_pal = &PALETTE[season];
    g3d_mesh_reset(&s->ground);
    g3d_mesh_reset(&s->tufts);
    build_ground(&s->ground);
    build_tufts(&s->tufts);
}

void llama_scene_free(llama_scene *s)
{
    g3d_mesh_free(&s->ground);
    g3d_mesh_free(&s->fence);
    g3d_mesh_free(&s->mountains);
    g3d_mesh_free(&s->tufts);
    g3d_mesh_free(&s->clouds);
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
    /* Las nubes van sin niebla: si no, se funden con el cielo y desaparecen. */
    g3d_ctx sky_ctx = *ctx;
    sky_ctx.light.fog_end = sky_ctx.light.fog_start;
    g3d_draw_mesh(t, &sky_ctx, &s->clouds, &id, tint * (1.f - night * 0.25f));
    g3d_draw_mesh(t, ctx, &s->mountains, &id, tint * 0.95f);
    g3d_draw_mesh(t, ctx, &s->ground, &id, tint);
    g3d_draw_mesh(t, ctx, &s->tufts, &id, tint);
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
    /* Dos capas: penumbra ancha y suave + nucleo mas oscuro. */
    g2d_blend_ellipse(t, (int)sx, (int)sy, (rx * 13) / 10, (ry * 13) / 10,
                      g3d_rgb(24, 44, 24), (uint8_t)(alpha / 3));
    g2d_blend_ellipse(t, (int)sx, (int)sy, rx, ry, g3d_rgb(20, 40, 20),
                      (uint8_t)((alpha * 2) / 3));
    g2d_blend_ellipse(t, (int)sx, (int)sy, (rx * 6) / 10, (ry * 6) / 10,
                      g3d_rgb(16, 34, 20), (uint8_t)(alpha / 2));
}
