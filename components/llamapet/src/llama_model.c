/*
 * Geometria y animacion de la llama.
 *
 * La llama mira hacia +Z. Todas las medidas estan en "unidades de corral"
 * (aproximadamente metros): una llama adulta mide ~1.6 de alto.
 */
#include "llama_model.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------- paleta */

#define COL_MUZZLE   g3d_rgb(124, 118, 118)
#define COL_NOSE     g3d_rgb( 78,  72,  72)
#define COL_HOOF     g3d_rgb( 86,  80,  80)
#define COL_EYE      g3d_rgb( 28,  24,  24)
#define COL_GLINT    g3d_rgb(255, 255, 255)
#define COL_EAR_IN   g3d_rgb(214, 152, 142)
#define COL_BLK_RED  g3d_rgb(178,  50,  44)
#define COL_BLK_TEAL g3d_rgb( 32, 132, 130)
#define COL_BLK_GOLD g3d_rgb(226, 206, 168)

/* --------------------------------------------------------- proporciones */

typedef struct {
    float body_l;    /* semilargo del torso */
    float body_r;    /* radio del torso */
    float neck_len;
    float neck_r;
    float head_s;    /* escala de la cabeza (la cria la tiene grande) */
    float leg_len;
    float scale;     /* escala general del bicho */
} llama_props;

static llama_props props_for(llama_stage s)
{
    switch (s) {
    case LLAMA_STAGE_CRIA: {
        llama_props p = { 0.33f, 0.28f, 0.29f, 0.155f, 1.28f, 0.24f, 0.74f };
        return p;
    }
    case LLAMA_STAGE_JOVEN: {
        llama_props p = { 0.44f, 0.34f, 0.43f, 0.180f, 1.10f, 0.36f, 0.88f };
        return p;
    }
    case LLAMA_STAGE_ANCIANA: {
        llama_props p = { 0.51f, 0.39f, 0.53f, 0.195f, 1.00f, 0.45f, 0.97f };
        return p;
    }
    default: {
        llama_props p = { 0.52f, 0.40f, 0.56f, 0.205f, 1.00f, 0.48f, 1.00f };
        return p;
    }
    }
}

/* ------------------------------------------------------------ construccion */

static void part_setup(llama_part *p, int parent, g3d_v3 pivot)
{
    p->parent  = parent;
    p->bind    = g3d_mat4_translate(pivot.x, pivot.y, pivot.z);
    p->local   = g3d_mat4_identity();
    p->world   = g3d_mat4_identity();
    p->visible = true;
    g3d_mesh_reset(&p->mesh);
}

bool llama_model_init(llama_model *m)
{
    memset(m, 0, sizeof(*m));
    /* Presupuesto de vertices/triangulos por parte (low-poly a proposito). */    /* Presupuesto de vertices/triangulos por parte. Las piezas curvas son
     * superficies de revolucion: mas triangulos que una caja, pero el costo
     * real del cuadro son los pixeles, no los vertices. */
    static const int vcap[LP_COUNT] = {
        /* cuerpo */ 96, /* lana */ 120, /* manta */ 300, /* cuello */ 56,
        /* cabeza */ 120, /* orejas */ 32, 32, /* ojos */ 40, 40,
        /* cola */ 32, /* patas */ 56, 56, 56, 56,
    };
    static const int tcap[LP_COUNT] = {
        144, 160, 300, 88, 200, 56, 56, 56, 56, 48, 80, 80, 80, 80,
    };
    for (int i = 0; i < LP_COUNT; i++) {
        if (!g3d_mesh_init(&m->parts[i].mesh, vcap[i], tcap[i])) {
            llama_model_free(m);
            return false;
        }
        m->parts[i].parent = -1;
        m->parts[i].bind = m->parts[i].local = m->parts[i].world = g3d_mat4_identity();
        m->parts[i].visible = true;
    }
    m->scale      = 1.f;
    m->wool_color = g3d_rgb(250, 232, 194);
    m->stage      = LLAMA_STAGE_COUNT; /* fuerza la primera construccion */
    m->blink_timer = 3.f;
    llama_model_configure(m, LLAMA_STAGE_ADULTA, false, true, m->wool_color);
    return true;
}

void llama_model_free(llama_model *m)
{
    for (int i = 0; i < LP_COUNT; i++) g3d_mesh_free(&m->parts[i].mesh);
}

/* Perfil del torso: fracciones del radio a lo largo del eje. */
static const float PROF_U[8] = { -1.00f, -0.86f, -0.58f, -0.20f,
                                  0.20f,  0.58f,  0.86f,  1.00f };
static const float PROF_R[8] = {  0.26f,  0.72f,  0.95f,  1.00f,
                                  0.99f,  0.92f,  0.74f,  0.30f };

/* Radio relativo del torso en la posicion `u` (-1 cola, +1 pecho). */
static float body_radius_at(float u)
{
    if (u <= PROF_U[0]) return PROF_R[0];
    for (int i = 1; i < 8; i++) {
        if (u <= PROF_U[i]) {
            float t = (u - PROF_U[i - 1]) / (PROF_U[i] - PROF_U[i - 1]);
            return PROF_R[i - 1] + (PROF_R[i] - PROF_R[i - 1]) * t;
        }
    }
    return PROF_R[7];
}

/* Gira una pieza construida sobre el eje Y para que quede tumbada sobre +Z
 * (el cuerpo y la cabeza se modelan "de pie" y despues se acuestan). */
static void lay_down(g3d_mesh *mesh, int first_v)
{
    g3d_mesh_transform_from(mesh, first_v, g3d_mat4_rot_x(1.5707963f));
}

void llama_model_configure(llama_model *m, llama_stage stage, bool sheared,
                           bool blanket, g3d_color wool)
{
    if (m->stage == stage && m->sheared == sheared && m->blanket == blanket &&
        m->wool_color == wool) {
        return;
    }
    m->stage      = stage;
    m->sheared    = sheared;
    m->blanket    = blanket;
    m->wool_color = wool;

    const llama_props p = props_for(stage);
    const g3d_color wc      = wool;
    const g3d_color wc_soft = g3d_color_shade(wool, 0.94f);
    const float BL = p.body_l;      /* semilargo del torso  */
    const float BR = p.body_r;      /* radio del torso      */

    /* --------------------------------------------------------------- cuerpo */
    llama_part *body = &m->parts[LP_BODY];
    part_setup(body, -1, g3d_v(0.f, p.leg_len + BR * 0.86f, 0.f));
    {
        g3d_ring rings[8];
        for (int i = 0; i < 8; i++) {
            rings[i].y  = PROF_U[i] * BL;
            rings[i].rx = PROF_R[i] * BR * 0.94f;
            rings[i].rz = PROF_R[i] * BR * 1.06f;
            rings[i].cx = 0.f;
            /* Lomo apenas arqueado (en espacio de construccion, -Z es arriba). */
            rings[i].cz = -BR * 0.05f * (1.f - PROF_U[i] * PROF_U[i]);
        }
        int v0 = body->mesh.nv;
        g3d_mesh_revolve(&body->mesh, rings, 8, 8, 0.f, 6.2831853f, wc,
                         G3D_CAP_LO | G3D_CAP_HI);
        lay_down(&body->mesh, v0);
    }

    /* ----------------------------------------------------------------- lana */
    llama_part *woolp = &m->parts[LP_WOOL];
    part_setup(woolp, LP_BODY, g3d_v(0.f, 0.f, 0.f));
    woolp->visible = !sheared;
    if (!sheared) {
        /* Mechones: rompen la silueta y le dan aire de peluche. */
        g3d_mesh_blob(&woolp->mesh, g3d_v(0.f, BR * 0.52f, -BL * 0.66f),
                      BR * 0.88f, BR * 0.46f, BL * 0.30f, 6, 2, wc);
        g3d_mesh_blob(&woolp->mesh, g3d_v(0.f, BR * 0.56f, BL * 0.66f),
                      BR * 0.82f, BR * 0.44f, BL * 0.26f, 6, 2, wc);
        g3d_mesh_blob(&woolp->mesh, g3d_v(-BR * 0.72f, -BR * 0.10f, -BL * 0.10f),
                      BR * 0.46f, BR * 0.62f, BL * 0.52f, 5, 2, wc_soft);
        g3d_mesh_blob(&woolp->mesh, g3d_v(BR * 0.72f, -BR * 0.10f, -BL * 0.10f),
                      BR * 0.46f, BR * 0.62f, BL * 0.52f, 5, 2, wc_soft);
        /* Panza. */
        g3d_mesh_blob(&woolp->mesh, g3d_v(0.f, -BR * 0.60f, 0.f),
                      BR * 0.82f, BR * 0.46f, BL * 0.72f, 5, 2, wc_soft);
    }

    /* ---------------------------------------------------------------- manta */
    llama_part *blk = &m->parts[LP_BLANKET];
    part_setup(blk, LP_BODY, g3d_v(0.f, 0.f, 0.f));
    blk->visible = blanket;
    if (blanket) {
        /* Cascara parcial que cae sobre los flancos. En espacio de construccion
         * el "arriba" del cuerpo cae en el angulo -PI/2. */
        /* Textil andino: la armamos celda por celda para lograr el zigzag de
         * colores, en vez de franjas lisas. */
        const g3d_color PAL[3] = { COL_BLK_RED, COL_BLK_TEAL, COL_BLK_GOLD };
        const float band_u[5] = { -0.62f, -0.34f, -0.04f, 0.26f, 0.52f };
        const float over  = sheared ? 1.06f : 1.24f;
        const float a_mid = -1.5707963f;
        const float a_half = 1.78f;
        const int   NSEG = 7;
        for (int b = 0; b < 4; b++) {
            g3d_ring band[2];
            for (int k = 0; k < 2; k++) {
                float u  = band_u[b + k];
                float rr = body_radius_at(u);
                band[k].y  = u * BL;
                band[k].rx = rr * BR * 0.94f * over;
                band[k].rz = rr * BR * 1.06f * over;
                band[k].cx = 0.f;
                band[k].cz = -BR * 0.05f;
            }
            for (int seg = 0; seg < NSEG; seg++) {
                float a0 = a_mid - a_half + (2.f * a_half) * seg / NSEG;
                float a1 = a_mid - a_half + (2.f * a_half) * (seg + 1) / NSEG;
                /* Rombos: el color depende de la diagonal banda+segmento. */
                int d = (b + seg) % 4;
                g3d_color c = (d == 0) ? PAL[1] : (d == 2 ? PAL[2] : PAL[0]);
                if (b == 0 || b == 3) c = (seg & 1) ? PAL[1] : PAL[0];
                int v0 = blk->mesh.nv;
                g3d_mesh_revolve(&blk->mesh, band, 2, 1, a0, a1, c, 0);
                lay_down(&blk->mesh, v0);
            }
        }
        /* Flecos: un borde claro que cuelga del filo inferior. */
        for (int e = 0; e < 2; e++) {
            float u  = band_u[e ? 4 : 0];
            float rr = body_radius_at(u);
            g3d_ring fr[2];
            for (int k = 0; k < 2; k++) {
                fr[k].y  = u * BL + (e ? 1.f : -1.f) * BL * 0.05f * k;
                fr[k].rx = rr * BR * 0.94f * over * (k ? 0.97f : 1.f);
                fr[k].rz = rr * BR * 1.06f * over * (k ? 0.97f : 1.f);
                fr[k].cx = 0.f;
                fr[k].cz = -BR * 0.05f;
            }
            int v0 = blk->mesh.nv;
            g3d_mesh_revolve(&blk->mesh, fr, 2, 7, a_mid - a_half, a_mid + a_half,
                             COL_BLK_GOLD, 0);
            lay_down(&blk->mesh, v0);
        }
    }

    /* --------------------------------------------------------------- cuello */
    llama_part *neck = &m->parts[LP_NECK];
    part_setup(neck, LP_BODY, g3d_v(0.f, BR * 0.52f, BL * 0.72f));
    {
        const float NL = p.neck_len, NR = p.neck_r;
        g3d_ring rings[6] = {
            { -BR * 0.52f, NR * 1.90f, NR * 1.95f, 0.f, -NL * 0.08f },
            {  NL * 0.14f, NR * 1.34f, NR * 1.38f, 0.f,  NL * 0.05f },
            {  NL * 0.38f, NR * 0.94f, NR * 0.96f, 0.f,  NL * 0.14f },
            {  NL * 0.62f, NR * 0.80f, NR * 0.82f, 0.f,  NL * 0.23f },
            {  NL * 0.85f, NR * 0.72f, NR * 0.74f, 0.f,  NL * 0.31f },
            {  NL * 1.00f, NR * 0.70f, NR * 0.72f, 0.f,  NL * 0.36f },
        };
        g3d_mesh_revolve(&neck->mesh, rings, 6, 6, 0.f, 6.2831853f, wc, G3D_CAP_LO);
    }

    /* --------------------------------------------------------------- cabeza */
    llama_part *head = &m->parts[LP_HEAD];
    part_setup(head, LP_NECK, g3d_v(0.f, p.neck_len * 0.98f, p.neck_len * 0.36f));
    const float hs = 0.195f * p.head_s;
    {
        /* Craneo: se modela a lo largo de Y y despues se acuesta hacia +Z. */
        g3d_ring skull[5] = {
            { -1.05f * hs, 0.34f * hs, 0.36f * hs, 0.f, 0.f },
            { -0.72f * hs, 0.74f * hs, 0.80f * hs, 0.f, 0.f },
            { -0.15f * hs, 0.82f * hs, 0.86f * hs, 0.f, 0.f },
            {  0.40f * hs, 0.72f * hs, 0.76f * hs, 0.f, 0.f },
            {  0.78f * hs, 0.52f * hs, 0.56f * hs, 0.f, 0.f },
        };
        int v0 = head->mesh.nv;
        g3d_mesh_revolve(&head->mesh, skull, 5, 6, 0.f, 6.2831853f, wc, G3D_CAP_LO);

        g3d_ring muzzle[4] = {
            { 0.52f * hs, 0.66f * hs, 0.70f * hs, 0.f, 0.02f * hs },
            { 0.95f * hs, 0.58f * hs, 0.58f * hs, 0.f, 0.06f * hs },
            { 1.35f * hs, 0.50f * hs, 0.48f * hs, 0.f, 0.10f * hs },
            { 1.62f * hs, 0.42f * hs, 0.38f * hs, 0.f, 0.12f * hs },
        };
        g3d_mesh_revolve(&head->mesh, muzzle, 4, 6, 0.f, 6.2831853f, COL_MUZZLE,
                         G3D_CAP_HI);
        lay_down(&head->mesh, v0);

        /* Ya en coordenadas de la cabeza: nariz y flequillo. */
        g3d_mesh_blob(&head->mesh, g3d_v(0.f, 0.14f * hs, 1.62f * hs),
                      0.26f * hs, 0.17f * hs, 0.14f * hs, 5, 2, COL_NOSE);
        if (!sheared) {
            g3d_mesh_blob(&head->mesh, g3d_v(0.f, 0.72f * hs, -0.05f * hs),
                          0.78f * hs, 0.50f * hs, 0.72f * hs, 6, 2, wc);
        }
    }

    /* --------------------------------------------------------------- orejas */
    for (int side = 0; side < 2; side++) {
        const float sx = side ? 1.f : -1.f;
        llama_part *ear = &m->parts[side ? LP_EAR_R : LP_EAR_L];
        part_setup(ear, LP_HEAD, g3d_v(sx * 0.40f * hs, 0.70f * hs, -0.22f * hs));
        g3d_ring rings[5] = {
            { 0.00f * hs, 0.30f * hs, 0.20f * hs, 0.f,             0.f },
            { 0.55f * hs, 0.31f * hs, 0.20f * hs, sx * 0.04f * hs, -0.02f * hs },
            { 1.05f * hs, 0.24f * hs, 0.15f * hs, sx * 0.10f * hs, -0.06f * hs },
            { 1.50f * hs, 0.13f * hs, 0.08f * hs, sx * 0.18f * hs, -0.12f * hs },
            { 1.80f * hs, 0.02f * hs, 0.02f * hs, sx * 0.26f * hs, -0.18f * hs },
        };
        g3d_mesh_revolve(&ear->mesh, rings, 5, 5, 0.f, 6.2831853f, wc, G3D_CAP_LO);
    }

    /* ----------------------------------------------------------------- ojos */
    for (int side = 0; side < 2; side++) {
        const float sx = side ? 1.f : -1.f;
        llama_part *eye = &m->parts[side ? LP_EYE_R : LP_EYE_L];
        part_setup(eye, LP_HEAD, g3d_v(sx * 0.60f * hs, 0.34f * hs, 0.50f * hs));
        g3d_mesh_blob(&eye->mesh, g3d_v(0.f, 0.f, 0.f),
                      0.36f * hs, 0.38f * hs, 0.34f * hs, 6, 2, COL_EYE);
        g3d_mesh_box(&eye->mesh, g3d_v(sx * 0.10f * hs, 0.15f * hs, 0.21f * hs),
                     g3d_v(0.10f * hs, 0.10f * hs, 0.06f * hs), COL_GLINT);
    }

    /* ----------------------------------------------------------------- cola */
    llama_part *tail = &m->parts[LP_TAIL];
    part_setup(tail, LP_BODY, g3d_v(0.f, BR * 0.42f, -BL * 0.92f));
    {
        g3d_ring rings[4] = {
            { 0.00f, BR * 0.30f, BR * 0.28f, 0.f, 0.f },
            { BR * 0.34f, BR * 0.24f, BR * 0.22f, 0.f, 0.f },
            { BR * 0.62f, BR * 0.15f, BR * 0.14f, 0.f, 0.f },
            { BR * 0.80f, BR * 0.04f, BR * 0.04f, 0.f, 0.f },
        };
        g3d_mesh_revolve(&tail->mesh, rings, 4, 5, 0.f, 6.2831853f, wc, G3D_CAP_LO);
    }

    /* ---------------------------------------------------------------- patas */
    static const int leg_ids[4] = { LP_LEG_FL, LP_LEG_FR, LP_LEG_BL, LP_LEG_BR };
    for (int i = 0; i < 4; i++) {
        const float sx = (i == 0 || i == 2) ? -1.f : 1.f;
        const float sz = (i < 2) ? 1.f : -1.f;
        llama_part *leg = &m->parts[leg_ids[i]];
        part_setup(leg, LP_BODY,
                   g3d_v(sx * BR * 0.60f, -BR * 0.62f, sz * BL * 0.60f));
        const float L = p.leg_len;
        const float k = BR / 0.30f;    /* escala general de la pata */
        g3d_ring rings[4] = {
            { -L,          0.085f * k, 0.088f * k, 0.f, 0.f },
            { -L * 0.62f,  0.082f * k, 0.086f * k, 0.f, 0.f },
            { -L * 0.28f,  0.098f * k, 0.112f * k, 0.f, 0.f },
            {  0.f,        0.150f * k, 0.180f * k, 0.f, 0.f },
        };
        g3d_mesh_revolve(&leg->mesh, rings, 4, 5, 0.f, 6.2831853f, wc, G3D_CAP_HI);

        g3d_ring hoof[2] = {
            { -L - 0.085f * k, 0.098f * k, 0.104f * k, 0.f, 0.f },
            { -L + 0.015f * k, 0.092f * k, 0.096f * k, 0.f, 0.f },
        };
        g3d_mesh_revolve(&leg->mesh, hoof, 2, 5, 0.f, 6.2831853f, COL_HOOF,
                         G3D_CAP_LO);
    }

    m->scale = p.scale;
}

/* -------------------------------------------------------------- animacion */

void llama_model_set_anim(llama_model *m, llama_anim a)
{
    if (m->anim == a) return;
    m->anim   = a;
    m->anim_t = 0.f;
}

static float ease_pulse(float x) /* 0..1 -> 0..1..0 */
{
    if (x <= 0.f || x >= 1.f) return 0.f;
    return sinf(x * (float)M_PI);
}

void llama_model_update(llama_model *m, float dt)
{
    m->t += dt;
    m->anim_t += dt;
    const float t = m->t;
    const llama_props p = props_for(m->stage);

    /* Parpadeo. */
    m->blink_timer -= dt;
    if (m->blink_timer <= 0.f) {
        m->blink_timer = 2.2f + llama_randf() * 4.f;
        m->blink_close = 1.f;
    }
    if (m->blink_close > 0.f) m->blink_close -= dt * 7.f;
    if (m->blink_close < 0.f) m->blink_close = 0.f;

    float body_y = 0.f, body_pitch = 0.f, body_roll = 0.f;
    float neck_pitch = -0.06f, neck_yaw = 0.f;
    float head_pitch = 0.f, head_yaw = 0.f;
    float tail_swing = sinf(t * 2.1f) * 0.22f;
    float ear_droop = 0.f;
    float leg[4] = { 0.f, 0.f, 0.f, 0.f };
    float eye_close = m->blink_close;

    switch (m->anim) {
    case LA_WALK: {
        const float w = t * 7.0f;
        leg[0] = sinf(w) * 0.55f;
        leg[3] = sinf(w) * 0.55f;
        leg[1] = sinf(w + (float)M_PI) * 0.55f;
        leg[2] = sinf(w + (float)M_PI) * 0.55f;
        body_y = fabsf(sinf(w)) * 0.035f;
        body_roll = sinf(w) * 0.035f;
        neck_pitch = -0.10f + sinf(w) * 0.05f;
        head_pitch = sinf(w * 0.5f) * 0.05f;
        tail_swing = sinf(w * 0.5f) * 0.35f;
        break;
    }
    case LA_EAT: {
        neck_pitch = 0.95f;
        head_pitch = 0.30f + sinf(t * 11.f) * 0.14f;
        body_pitch = 0.05f;
        tail_swing = sinf(t * 3.f) * 0.4f;
        break;
    }
    case LA_SLEEP: {
        /* Postura "kush": patas plegadas bajo el cuerpo. */
        body_y = -p.leg_len * 0.82f;
        for (int i = 0; i < 4; i++) leg[i] = (i < 2 ? 1.05f : -1.05f);
        neck_pitch = 0.18f + sinf(t * 0.8f) * 0.05f;
        head_pitch = -0.22f;
        head_yaw   = 0.65f;
        tail_swing = 0.f;
        eye_close  = 1.f;
        ear_droop  = 0.35f;
        break;
    }
    case LA_HAPPY: {
        const float hop = fabsf(sinf(t * 5.2f));
        body_y = hop * 0.30f;
        body_pitch = -0.12f * hop;
        for (int i = 0; i < 4; i++) leg[i] = (i < 2 ? 0.7f : -0.5f) * hop;
        neck_pitch = -0.28f - hop * 0.15f;
        head_pitch = -0.15f;
        tail_swing = sinf(t * 9.f) * 0.5f;
        break;
    }
    case LA_SICK: {
        body_y = -0.05f + sinf(t * 1.1f) * 0.012f;
        neck_pitch = 0.55f + sinf(t * 0.9f) * 0.06f;
        head_pitch = 0.20f;
        ear_droop = 0.75f;
        tail_swing = sinf(t * 0.7f) * 0.06f;
        break;
    }
    case LA_SPIT: {
        const float k = m->anim_t / 0.9f;
        if (k < 0.4f) {
            neck_pitch = -0.10f - k * 1.2f;
        } else if (k < 0.62f) {
            neck_pitch = -0.58f + (k - 0.4f) * 6.0f;
            head_pitch = -0.35f;
        } else {
            neck_pitch = 0.74f - (k - 0.62f) * 2.0f;
        }
        tail_swing = sinf(t * 7.f) * 0.4f;
        break;
    }
    case LA_SHEAR: {
        body_y = sinf(t * 14.f) * 0.02f;
        neck_pitch = -0.2f;
        head_yaw = sinf(t * 8.f) * 0.25f;
        ear_droop = 0.4f;
        break;
    }
    case LA_DEAD: {
        body_y = -p.leg_len * 0.85f;
        for (int i = 0; i < 4; i++) leg[i] = (i < 2 ? 1.15f : -1.15f);
        neck_pitch = 0.9f;
        head_pitch = -0.2f;
        eye_close = 1.f;
        ear_droop = 1.0f;
        tail_swing = 0.f;
        break;
    }
    case LA_IDLE:
    default: {
        body_y = sinf(t * 1.6f) * 0.012f;
        neck_pitch = -0.06f + sinf(t * 0.9f) * 0.05f;
        head_pitch = sinf(t * 1.3f + 1.f) * 0.06f;
        head_yaw = sinf(t * 0.37f) * 0.22f;
        /* Cada tanto sacude una oreja. */
        m->ear_twitch -= dt;
        if (m->ear_twitch < -1.2f) m->ear_twitch = 3.f + llama_randf() * 4.f;
        break;
    }
    }

    m->body_y = body_y;

    /* --- composicion de matrices --- */
    llama_part *P = m->parts;

    g3d_mat4 root = g3d_mat4_mul(g3d_mat4_translate(m->pos.x, m->pos.y, m->pos.z),
                                 g3d_mat4_rot_y(m->heading));
    root = g3d_mat4_mul(root, g3d_mat4_scale(m->scale, m->scale, m->scale));

    P[LP_BODY].local = g3d_mat4_mul(g3d_mat4_translate(0.f, body_y, 0.f),
                                    g3d_mat4_mul(g3d_mat4_rot_x(body_pitch),
                                                 g3d_mat4_rot_z(body_roll)));
    P[LP_WOOL].local    = g3d_mat4_identity();
    P[LP_BLANKET].local = g3d_mat4_identity();
    P[LP_NECK].local    = g3d_mat4_rot_x(neck_pitch);
    P[LP_NECK].local    = g3d_mat4_mul(P[LP_NECK].local, g3d_mat4_rot_y(neck_yaw));
    P[LP_HEAD].local    = g3d_mat4_mul(g3d_mat4_rot_y(head_yaw), g3d_mat4_rot_x(head_pitch));
    P[LP_TAIL].local    = g3d_mat4_mul(g3d_mat4_rot_z(tail_swing),
                                       g3d_mat4_rot_x(-0.5f));

    const float twitch = (m->ear_twitch < 0.f) ? ease_pulse(-m->ear_twitch / 1.2f) : 0.f;
    P[LP_EAR_L].local = g3d_mat4_mul(g3d_mat4_rot_z(0.07f + ear_droop * 1.1f),
                                     g3d_mat4_rot_x(-0.12f - twitch * 0.6f));
    P[LP_EAR_R].local = g3d_mat4_mul(g3d_mat4_rot_z(-0.07f - ear_droop * 1.1f),
                                     g3d_mat4_rot_x(-0.12f));

    const float eye_s = 1.f - eye_close * 0.88f;
    P[LP_EYE_L].local = g3d_mat4_scale(1.f, eye_s, 1.f);
    P[LP_EYE_R].local = g3d_mat4_scale(1.f, eye_s, 1.f);

    static const int leg_ids[4] = { LP_LEG_FL, LP_LEG_FR, LP_LEG_BL, LP_LEG_BR };
    for (int i = 0; i < 4; i++) P[leg_ids[i]].local = g3d_mat4_rot_x(leg[i]);

    /* --- jerarquia --- */
    for (int i = 0; i < LP_COUNT; i++) {
        g3d_mat4 lm = g3d_mat4_mul(P[i].bind, P[i].local);
        if (P[i].parent < 0) {
            P[i].world = g3d_mat4_mul(root, lm);
        } else {
            P[i].world = g3d_mat4_mul(P[P[i].parent].world, lm);
        }
    }
}

void llama_model_draw(g3d_target *t, const g3d_ctx *ctx, const llama_model *m, float tint)
{
    for (int i = 0; i < LP_COUNT; i++) {
        if (!m->parts[i].visible || m->parts[i].mesh.nt == 0) continue;
        g3d_draw_mesh(t, ctx, &m->parts[i].mesh, &m->parts[i].world, tint);
    }
}

g3d_v3 llama_model_head_pos(const llama_model *m)
{
    return g3d_mat4_point(&m->parts[LP_HEAD].world, g3d_v(0.f, 0.20f, 0.f));
}

g3d_v3 llama_model_mouth_pos(const llama_model *m)
{
    return g3d_mat4_point(&m->parts[LP_HEAD].world, g3d_v(0.f, 0.05f, 0.34f));
}

g3d_v3 llama_model_body_pos(const llama_model *m)
{
    return g3d_mat4_point(&m->parts[LP_BODY].world, g3d_v(0.f, 0.f, 0.f));
}
