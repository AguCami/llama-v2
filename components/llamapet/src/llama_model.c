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

#define COL_MUZZLE   g3d_rgb(196, 170, 140)
#define COL_NOSE     g3d_rgb( 64,  54,  52)
#define COL_HOOF     g3d_rgb( 72,  64,  60)
#define COL_EYE      g3d_rgb( 28,  24,  24)
#define COL_GLINT    g3d_rgb(255, 255, 255)
#define COL_EAR_IN   g3d_rgb(214, 152, 142)
#define COL_BLK_RED  g3d_rgb(204,  62,  56)
#define COL_BLK_TEAL g3d_rgb( 38, 150, 148)
#define COL_BLK_GOLD g3d_rgb(232, 178,  60)

/* --------------------------------------------------------- proporciones */

typedef struct {
    float body_w, body_h, body_l;
    float neck_len, neck_w;
    float head_s;
    float leg_len;
    float scale;
} llama_props;

static llama_props props_for(llama_stage s)
{
    switch (s) {
    case LLAMA_STAGE_CRIA: {
        llama_props p = { 0.22f, 0.22f, 0.34f, 0.34f, 0.16f, 1.32f, 0.34f, 0.74f };
        return p;
    }
    case LLAMA_STAGE_JOVEN: {
        llama_props p = { 0.26f, 0.26f, 0.46f, 0.55f, 0.18f, 1.12f, 0.50f, 0.86f };
        return p;
    }
    case LLAMA_STAGE_ANCIANA: {
        llama_props p = { 0.30f, 0.29f, 0.55f, 0.70f, 0.21f, 1.02f, 0.62f, 0.97f };
        return p;
    }
    default: {
        llama_props p = { 0.29f, 0.30f, 0.56f, 0.76f, 0.21f, 1.00f, 0.66f, 1.00f };
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
    /* Presupuesto de vertices/triangulos por parte (low-poly a proposito). */
    static const int vcap[LP_COUNT] = { 80, 80, 64, 24, 64, 16, 16, 24, 24, 16, 24, 24, 24, 24 };
    static const int tcap[LP_COUNT] = { 96, 96, 72, 24, 72, 12, 12, 24, 24, 12, 24, 24, 24, 24 };
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
    m->wool_color = g3d_rgb(238, 226, 200);
    m->stage      = LLAMA_STAGE_COUNT; /* fuerza la primera construccion */
    m->blink_timer = 3.f;
    llama_model_configure(m, LLAMA_STAGE_ADULTA, false, true, m->wool_color);
    return true;
}

void llama_model_free(llama_model *m)
{
    for (int i = 0; i < LP_COUNT; i++) g3d_mesh_free(&m->parts[i].mesh);
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
    const g3d_color wc = wool;
    const g3d_color wc_dark = g3d_color_shade(wool, 0.88f);

    /* ---- cuerpo ---- */
    llama_part *body = &m->parts[LP_BODY];
    part_setup(body, -1, g3d_v(0.f, p.leg_len + p.body_h * 0.72f, 0.f));
    g3d_mesh_box(&body->mesh, g3d_v(0.f, 0.f, 0.f),
                 g3d_v(p.body_w, p.body_h, p.body_l), wc);
    /* Pecho un poco mas ancho adelante. */
    g3d_mesh_box(&body->mesh, g3d_v(0.f, 0.02f, p.body_l * 0.72f),
                 g3d_v(p.body_w * 0.92f, p.body_h * 0.86f, p.body_l * 0.28f), wc_dark);
    /* Grupa. */
    g3d_mesh_box(&body->mesh, g3d_v(0.f, 0.04f, -p.body_l * 0.80f),
                 g3d_v(p.body_w * 0.86f, p.body_h * 0.80f, p.body_l * 0.24f), wc_dark);

    /* ---- lana (mechones) ---- */
    llama_part *woolp = &m->parts[LP_WOOL];
    part_setup(woolp, LP_BODY, g3d_v(0.f, 0.f, 0.f));
    woolp->visible = !sheared;
    if (!sheared) {
        const float bw = p.body_w, bh = p.body_h, bl = p.body_l;
        g3d_mesh_box(&woolp->mesh, g3d_v(0.f, bh * 0.92f, bl * 0.30f),
                     g3d_v(bw * 0.80f, bh * 0.26f, bl * 0.30f), wc);
        g3d_mesh_box(&woolp->mesh, g3d_v(0.f, bh * 0.95f, -bl * 0.35f),
                     g3d_v(bw * 0.86f, bh * 0.24f, bl * 0.34f), wc);
        g3d_mesh_box(&woolp->mesh, g3d_v(0.f, bh * 1.10f, -bl * 0.05f),
                     g3d_v(bw * 0.62f, bh * 0.20f, bl * 0.28f), wc);
        g3d_mesh_box(&woolp->mesh, g3d_v(0.f, -bh * 0.80f, -bl * 0.10f),
                     g3d_v(bw * 0.86f, bh * 0.22f, bl * 0.72f), wc_dark);
    }

    /* ---- manta andina ---- */
    llama_part *blk = &m->parts[LP_BLANKET];
    part_setup(blk, LP_BODY, g3d_v(0.f, 0.f, 0.f));
    blk->visible = blanket;
    if (blanket) {
        const float y = p.body_h * (sheared ? 1.02f : 1.20f);
        const float w = p.body_w * (sheared ? 1.06f : 1.06f);
        const g3d_color stripes[3] = { COL_BLK_RED, COL_BLK_GOLD, COL_BLK_TEAL };
        for (int i = 0; i < 3; i++) {
            float z = p.body_l * (-0.34f + i * 0.30f);
            g3d_mesh_box(&blk->mesh, g3d_v(0.f, y, z),
                         g3d_v(w, p.body_h * 0.10f, p.body_l * 0.13f), stripes[i]);
        }
        /* Faldones a los costados. */
        g3d_mesh_box(&blk->mesh, g3d_v(-w, y - p.body_h * 0.30f, 0.f),
                     g3d_v(p.body_w * 0.06f, p.body_h * 0.34f, p.body_l * 0.42f), COL_BLK_RED);
        g3d_mesh_box(&blk->mesh, g3d_v(w, y - p.body_h * 0.30f, 0.f),
                     g3d_v(p.body_w * 0.06f, p.body_h * 0.34f, p.body_l * 0.42f), COL_BLK_RED);
    }

    /* ---- cuello ---- */
    llama_part *neck = &m->parts[LP_NECK];
    part_setup(neck, LP_BODY, g3d_v(0.f, p.body_h * 0.70f, p.body_l * 0.74f));
    g3d_mesh_frustum(&neck->mesh, g3d_v(0.f, -0.04f, 0.f),
                     p.neck_w * 1.15f, p.neck_w * 1.10f,
                     p.neck_w * 0.82f, p.neck_w * 0.80f,
                     p.neck_len, g3d_v(0.f, 0.f, p.neck_len * 0.16f), wc);

    /* ---- cabeza ---- */
    llama_part *head = &m->parts[LP_HEAD];
    part_setup(head, LP_NECK, g3d_v(0.f, p.neck_len - 0.02f, p.neck_len * 0.16f));
    const float hs = 0.155f * p.head_s * (p.scale > 0.9f ? 1.f : 1.f);
    g3d_mesh_box(&head->mesh, g3d_v(0.f, hs * 0.55f, 0.f),
                 g3d_v(hs, hs * 0.90f, hs * 1.15f), wc);
    /* Hocico. */
    g3d_mesh_box(&head->mesh, g3d_v(0.f, hs * 0.20f, hs * 1.55f),
                 g3d_v(hs * 0.62f, hs * 0.52f, hs * 0.50f), COL_MUZZLE);
    /* Nariz. */
    g3d_mesh_box(&head->mesh, g3d_v(0.f, hs * 0.42f, hs * 2.02f),
                 g3d_v(hs * 0.34f, hs * 0.20f, hs * 0.08f), COL_NOSE);
    /* Flequillo. */
    if (!sheared) {
        g3d_mesh_box(&head->mesh, g3d_v(0.f, hs * 1.42f, hs * 0.30f),
                     g3d_v(hs * 0.92f, hs * 0.30f, hs * 0.70f), wc);
    }

    /* ---- orejas ---- */
    for (int side = 0; side < 2; side++) {
        const float sx = side ? 1.f : -1.f;
        llama_part *ear = &m->parts[side ? LP_EAR_R : LP_EAR_L];
        part_setup(ear, LP_HEAD, g3d_v(sx * hs * 0.62f, hs * 1.35f, -hs * 0.10f));
        g3d_mesh_pyramid(&ear->mesh, g3d_v(0.f, 0.f, 0.f),
                         hs * 0.42f, hs * 0.34f, hs * 2.05f,
                         g3d_v(sx * hs * 0.20f, 0.f, -hs * 0.16f), wc);
        g3d_mesh_pyramid(&ear->mesh, g3d_v(0.f, hs * 0.05f, hs * 0.06f),
                         hs * 0.20f, hs * 0.14f, hs * 1.45f,
                         g3d_v(sx * hs * 0.14f, 0.f, -hs * 0.10f), COL_EAR_IN);
    }

    /* ---- ojos ---- */
    for (int side = 0; side < 2; side++) {
        const float sx = side ? 1.f : -1.f;
        llama_part *eye = &m->parts[side ? LP_EYE_R : LP_EYE_L];
        part_setup(eye, LP_HEAD, g3d_v(sx * hs * 0.84f, hs * 0.82f, hs * 0.62f));
        g3d_mesh_box(&eye->mesh, g3d_v(0.f, 0.f, 0.f),
                     g3d_v(hs * 0.20f, hs * 0.26f, hs * 0.20f), COL_EYE);
        g3d_mesh_box(&eye->mesh, g3d_v(sx * hs * 0.06f, hs * 0.12f, hs * 0.16f),
                     g3d_v(hs * 0.07f, hs * 0.08f, hs * 0.06f), COL_GLINT);
    }

    /* ---- cola ---- */
    llama_part *tail = &m->parts[LP_TAIL];
    part_setup(tail, LP_BODY, g3d_v(0.f, p.body_h * 0.55f, -p.body_l * 1.00f));
    g3d_mesh_frustum(&tail->mesh, g3d_v(0.f, 0.f, 0.f),
                     p.body_w * 0.22f, p.body_w * 0.20f,
                     p.body_w * 0.10f, p.body_w * 0.09f,
                     p.body_h * 0.60f, g3d_v(0.f, 0.f, -p.body_h * 0.22f), wc);

    /* ---- patas ---- */
    static const int leg_ids[4] = { LP_LEG_FL, LP_LEG_FR, LP_LEG_BL, LP_LEG_BR };
    for (int i = 0; i < 4; i++) {
        const float sx = (i == 0 || i == 2) ? -1.f : 1.f;
        const float sz = (i < 2) ? 1.f : -1.f;
        llama_part *leg = &m->parts[leg_ids[i]];
        part_setup(leg, LP_BODY,
                   g3d_v(sx * p.body_w * 0.66f, -p.body_h * 0.62f, sz * p.body_l * 0.62f));
        const float len = p.leg_len - p.body_h * 0.10f;
        g3d_mesh_frustum(&leg->mesh, g3d_v(0.f, -len, 0.f),
                         p.body_w * 0.34f, p.body_w * 0.34f,
                         p.body_w * 0.42f, p.body_w * 0.42f,
                         len, g3d_v(0.f, 0.f, 0.f), wc);
        g3d_mesh_box(&leg->mesh, g3d_v(0.f, -len - p.body_w * 0.06f, 0.f),
                     g3d_v(p.body_w * 0.20f, p.body_w * 0.07f, p.body_w * 0.22f), COL_HOOF);
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
        body_y = -(p.leg_len - p.body_h * 0.30f);
        for (int i = 0; i < 4; i++) leg[i] = (i < 2 ? 1.48f : -1.48f);
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
        body_y = -(p.leg_len - p.body_h * 0.25f);
        for (int i = 0; i < 4; i++) leg[i] = (i < 2 ? 1.5f : -1.5f);
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
    P[LP_EAR_L].local = g3d_mat4_mul(g3d_mat4_rot_z(0.18f + ear_droop * 1.1f),
                                     g3d_mat4_rot_x(-0.12f - twitch * 0.6f));
    P[LP_EAR_R].local = g3d_mat4_mul(g3d_mat4_rot_z(-0.18f - ear_droop * 1.1f),
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
