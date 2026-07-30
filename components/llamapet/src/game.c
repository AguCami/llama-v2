/*
 * Bucle del juego: entrada tactil/IMU, comportamiento de la llama,
 * camara, particulas y orquestacion del render.
 */
#include "game_internal.h"
#include "llama_platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SKY_DAY_TOP    g3d_rgb(104, 176, 236)
#define SKY_DAY_BOT    g3d_rgb(206, 232, 246)
#define SKY_NIGHT_TOP  g3d_rgb( 12,  16,  46)
#define SKY_NIGHT_BOT  g3d_rgb( 62,  58, 104)
#define WOOL_CREAM     g3d_rgb(240, 224, 184)
#define WOOL_GREY      g3d_rgb(224, 214, 200)

static const char *NAMES[] = {
    "Pelusa", "Copito", "Tito", "Nube", "Chispa", "Lupita", "Cuzco", "Ramona",
};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float approach(float v, float target, float rate, float dt)
{
    float d = target - v;
    float step = rate * dt;
    if (d > step)  return v + step;
    if (d < -step) return v - step;
    return target;
}

/* --------------------------------------------------------------- toasts */

void game_toast(llama_game *g, const char *msg)
{
    strncpy(g->toast, msg, sizeof(g->toast) - 1);
    g->toast[sizeof(g->toast) - 1] = 0;
    g->toast_t = 2.6f;
}

/* ------------------------------------------------------------ particulas */

void game_emit(llama_game *g, particle_kind kind, g3d_v3 pos, int count)
{
    for (int n = 0; n < count; n++) {
        particle *p = NULL;
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (!g->particles[i].used) { p = &g->particles[i]; break; }
        }
        if (!p) return;
        p->used = true;
        p->kind = (uint8_t)kind;
        p->pos  = pos;
        p->pos.x += (llama_randf() - 0.5f) * 0.24f;
        p->pos.z += (llama_randf() - 0.5f) * 0.24f;
        p->size = 1.f;

        switch (kind) {
        case PT_HEART:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.3f, 0.65f + llama_randf() * 0.3f,
                           (llama_randf() - 0.5f) * 0.3f);
            p->life = p->life0 = 1.6f;
            break;
        case PT_ZZZ:
            p->vel = g3d_v(0.18f, 0.34f, 0.f);
            p->life = p->life0 = 2.6f;
            break;
        case PT_STAR:
            p->vel = g3d_v((llama_randf() - 0.5f) * 1.1f, 0.5f + llama_randf() * 0.7f,
                           (llama_randf() - 0.5f) * 1.1f);
            p->life = p->life0 = 1.1f;
            break;
        case PT_SPIT:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.4f, 1.1f, 2.6f);
            p->life = p->life0 = 0.9f;
            break;
        case PT_CRUMB:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.8f, 0.5f, 0.4f);
            p->life = p->life0 = 0.7f;
            break;
        case PT_ANGRY:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.6f, 0.9f, 0.f);
            p->life = p->life0 = 0.9f;
            break;
        case PT_COIN:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.5f, 1.5f, 0.f);
            p->life = p->life0 = 1.2f;
            break;
        case PT_SNOW:
            p->vel = g3d_v((llama_randf() - 0.5f) * 0.35f, -0.55f - llama_randf() * 0.3f,
                           (llama_randf() - 0.5f) * 0.2f);
            p->life = p->life0 = 6.f;
            break;
        case PT_LEAF:
            p->vel = g3d_v(0.5f + llama_randf() * 0.6f, -0.75f - llama_randf() * 0.35f,
                           (llama_randf() - 0.5f) * 0.3f);
            p->life = p->life0 = 5.f;
            break;
        case PT_PETAL:
            p->vel = g3d_v(0.35f + llama_randf() * 0.4f, -0.45f - llama_randf() * 0.25f,
                           (llama_randf() - 0.5f) * 0.3f);
            p->life = p->life0 = 6.f;
            break;
        case PT_RAIN:
            /* Cae rapido y casi recta: el viento la inclina despues. */
            p->vel = g3d_v(0.f, -7.5f - llama_randf() * 2.5f, 0.f);
            p->life = p->life0 = 1.4f;
            break;
        default:
            p->vel = g3d_v(0.f, 0.6f, 0.f);
            p->life = p->life0 = 1.3f;
            break;
        }
    }
}

void game_particles_update(llama_game *g, float dt)
{
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle *p = &g->particles[i];
        if (!p->used) continue;
        p->life -= dt;
        if (p->life <= 0.f) { p->used = false; continue; }
        p->pos = g3d_v3_add(p->pos, g3d_v3_mul(p->vel, dt));
        if (p->kind == PT_SPIT || p->kind == PT_CRUMB || p->kind == PT_COIN) {
            p->vel.y -= 4.2f * dt;   /* gravedad */
        } else if (PT_IS_WEATHER(p->kind)) {
            /* Caida con vaiven lateral; se apagan al tocar el piso. */
            const float wind = g->wind + g->gust;
            if (p->kind == PT_RAIN) {
                p->vel.x = wind * 0.55f;      /* la gota no revolotea, se inclina */
            } else {
                p->vel.x += (sinf(p->life * 3.1f + p->pos.z) * 0.6f + wind) * dt;
            }
            if (p->pos.y <= 0.03f) p->used = false;
        } else {
            p->vel.y *= (1.f - 0.6f * dt);
        }
    }
}

static void draw_particle_icon(g3d_target *t, int kind, int x, int y, int s,
                               float fade, int slant)
{
    if (s < 2) s = 2;
    switch (kind) {
    case PT_HEART: {
        g3d_color c = g3d_color_shade(g3d_rgb(244, 84, 118), 0.5f + fade * 0.5f);
        g2d_fill_circle(t, x - s / 3, y - s / 4, s / 2, c);
        g2d_fill_circle(t, x + s / 3, y - s / 4, s / 2, c);
        g2d_fill_tri(t, x - s, y, x + s, y, x, y + s, c);
        break;
    }
    case PT_ZZZ: {
        g3d_color c = g3d_rgb(230, 240, 255);
        char z[2] = { 'Z', 0 };
        int sc = s > 6 ? 2 : 1;
        g2d_text(t, x - 3 * sc, y - 4 * sc, sc, c, z);
        break;
    }
    case PT_STAR: {
        g3d_color c = g3d_rgb(255, 236, 130);
        g2d_vline(t, x, y - s, s * 2, c);
        g2d_hline(t, x - s, y, s * 2, c);
        g2d_pixel(t, x - s / 2, y - s / 2, c);
        g2d_pixel(t, x + s / 2, y + s / 2, c);
        g2d_pixel(t, x + s / 2, y - s / 2, c);
        g2d_pixel(t, x - s / 2, y + s / 2, c);
        break;
    }
    case PT_SPIT:
        g2d_fill_ellipse(t, x, y, s, (s * 3) / 4, g3d_rgb(150, 220, 180));
        break;
    case PT_CRUMB:
        g2d_fill_rect(t, x - s / 2, y - s / 2, s, s, g3d_rgb(214, 178, 90));
        break;
    case PT_ANGRY: {
        g3d_color c = g3d_rgb(244, 96, 72);
        g2d_line(t, x - s, y - s, x + s, y + s, c);
        g2d_line(t, x + s, y - s, x - s, y + s, c);
        break;
    }
    case PT_COIN:
        g2d_fill_circle(t, x, y, s, g3d_rgb(248, 206, 70));
        g2d_circle(t, x, y, s, g3d_rgb(190, 146, 30));
        break;
    case PT_SNOW: {
        int r = s > 3 ? 2 : 1;
        g2d_fill_circle(t, x, y, r, g3d_rgb(250, 252, 255));
        break;
    }
    case PT_LEAF: {
        g3d_color c = (((int)(x + y)) & 1) ? g3d_rgb(206, 126, 48)
                                          : g3d_rgb(176, 92, 40);
        g2d_fill_ellipse(t, x, y, s > 3 ? 3 : 2, s > 3 ? 2 : 1, c);
        break;
    }
    case PT_PETAL:
        g2d_fill_ellipse(t, x, y, s > 3 ? 2 : 1, s > 3 ? 3 : 2,
                         g3d_rgb(250, 214, 228));
        break;
    case PT_RAIN: {
        /* Un trazo: alto segun la distancia e inclinado por el viento. */
        int len = s > 4 ? 5 : (s > 2 ? 4 : 3);
        g2d_line(t, x - slant, y - len, x, y, g3d_rgb(176, 206, 236));
        break;
    }
    default:
        g2d_fill_circle(t, x, y, s, g3d_rgb(255, 255, 255));
        break;
    }
}

void game_particles_draw(llama_game *g, g3d_target *t)
{
    const int slant = (int)((g->wind + g->gust) * 0.55f + (g->wind < 0.f ? -0.5f : 0.5f));
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle *p = &g->particles[i];
        if (!p->used) continue;
        float sx, sy, scale;
        if (!g3d_project(t, &g->ctx, p->pos, &sx, &sy, &scale)) continue;
        float fade = p->life / p->life0;
        int size = (int)(0.09f * scale * p->size);
        draw_particle_icon(t, p->kind, (int)sx, (int)sy, size, fade, slant);
    }
}

/* ----------------------------------------------------------------- clima */

/* Probabilidad de que se largue a llover, por estacion. En invierno cae nieve
 * en vez de agua, asi que ahi la lluvia queda en cero. */
static float rain_chance(llama_season s)
{
    switch (s) {
    case SEASON_PRIMAVERA: return 0.40f;
    case SEASON_OTONIO:    return 0.34f;
    case SEASON_VERANO:    return 0.16f;   /* tormenta corta de tarde */
    default:               return 0.f;
    }
}

/*
 * El tiempo no viene del reloj: se sortea y se mueve despacio, asi que dos
 * ratos de juego no se parecen. El viento cambia cada medio minuto y la lluvia
 * cada varios minutos; los dos se interpolan para que no haya saltos.
 */
static void wind_and_rain_update(llama_game *g, float dt)
{
    g->wind_timer -= dt;
    if (g->wind_timer <= 0.f) {
        g->wind_timer = 12.f + llama_randf() * 26.f;
        /* Base del viento, con el signo del sorteo anterior mas o menos mantenido. */
        float mag = llama_randf() * llama_randf() * 3.2f;    /* casi siempre suave */
        g->wind_target = (llama_randf() < 0.5f) ? -mag : mag;
        if (g->rain > 0.4f) g->wind_target *= 1.6f;          /* con lluvia sopla mas */
    }
    g->rain_timer -= dt;
    if (g->rain_timer <= 0.f) {
        g->rain_timer = 90.f + llama_randf() * 150.f;
        float chance = rain_chance(g->amb.season);
        g->rain_target = (llama_randf() < chance)
                             ? 0.45f + llama_randf() * 0.55f
                             : 0.f;
    }

    g->wind = approach(g->wind, g->wind_target, 0.35f, dt);
    g->rain = approach(g->rain, g->rain_target, 0.20f, dt);

    /* Rafagas: golpes cortos encima de la base, mas seguidos si ya hay viento. */
    g->gust *= (1.f - 1.8f * dt);
    if (g->gust < 0.02f && g->gust > -0.02f) g->gust = 0.f;
    float gust_rate = (0.10f + fabsf(g->wind) * 0.12f + g->rain * 0.15f) * dt;
    if (llama_randf() < gust_rate) {
        float k = 0.8f + llama_randf() * 1.6f;
        g->gust = (g->wind >= 0.f) ? k : -k;
    }
}

/* Nieve, hojas, petalos o lluvia. Se limita la cantidad para no quedarse con
 * todas las ranuras de particulas y tapar corazones y Zzz. */
static void weather_update(llama_game *g, float dt)
{
    int kind, budget;
    float per_second;

    wind_and_rain_update(g, dt);

    /* Al cambiar de estacion hay que barrer lo que quedo cayendo: si no, se
     * ven copos de nieve sobre el pastizal de otonio hasta que se apaguen. */
    if (g->weather_season != (int)g->amb.season) {
        g->weather_season = (int)g->amb.season;
        g->weather_timer = 0.f;
        g->rain_timer = 0.f;               /* vuelve a sortear con la estacion nueva */
        for (int i = 0; i < MAX_PARTICLES; i++) {
            particle *p = &g->particles[i];
            if (p->used && PT_IS_WEATHER(p->kind)) p->used = false;
        }
    }

    if (g->rain > 0.15f) {
        /* Mientras llueve manda el agua: ni petalos ni hojas al mismo tiempo. */
        kind = PT_RAIN;
        per_second = 6.f + g->rain * 26.f;
        budget = 8 + (int)(g->rain * 16.f);
    } else {
        switch (g->amb.season) {
        case SEASON_INVIERNO:  kind = PT_SNOW;  per_second = 7.f;  budget = 18; break;
        case SEASON_OTONIO:    kind = PT_LEAF;  per_second = 2.6f; budget = 10; break;
        case SEASON_PRIMAVERA: kind = PT_PETAL; per_second = 1.8f; budget = 8;  break;
        default: return;                      /* en verano el cielo esta limpio */
        }
    }

    int active = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        const particle *p = &g->particles[i];
        if (p->used && PT_IS_WEATHER(p->kind)) active++;
    }
    if (active >= budget) return;

    g->weather_timer += dt * per_second;
    while (g->weather_timer >= 1.f && active < budget) {
        g->weather_timer -= 1.f;
        g3d_v3 at = g3d_v(g->model.pos.x + (llama_randf() - 0.5f) * 7.f,
                          3.2f + llama_randf() * 1.2f,
                          g->model.pos.z + (llama_randf() - 0.5f) * 6.f);
        game_emit(g, (particle_kind)kind, at, 1);
        active++;
    }
}

/* ---------------------------------------------------------------- camara */

static void update_camera(llama_game *g, float dt)
{
    g->cam_yaw += g->cam_yaw_vel * dt;
    g->cam_yaw_vel *= (1.f - 2.6f * dt);
    if (fabsf(g->cam_yaw_vel) < 0.01f) g->cam_yaw_vel = 0.f;

    if (g->cam_shake > 0.f) g->cam_shake -= dt * 2.f;
    if (g->cam_shake < 0.f) g->cam_shake = 0.f;

    float yaw = g->cam_yaw + g->tilt_x * 0.35f;
    float dist = g->cam_dist;
    float hgt  = g->cam_height + g->tilt_y * 0.4f;

    g3d_v3 focus = g3d_v(g->model.pos.x * 0.5f, g->cam_target_y, g->model.pos.z * 0.5f);
    g3d_v3 eye = g3d_v(focus.x + sinf(yaw) * dist,
                       hgt + g->cam_shake * (llama_randf() - 0.5f) * 0.3f,
                       focus.z + cosf(yaw) * dist);

    float aspect = (float)g->w / (float)g->h;
    g3d_ctx_camera(&g->ctx, eye, focus, 0.72f, aspect, 0.15f, 60.f);
}

/* ------------------------------------------------------------ apariencia */

static void sync_appearance(llama_game *g)
{
    llama_stage st = llama_pet_stage(&g->pet);
    bool sheared = g->pet.wool < 25.f;
    g3d_color wool = (st == LLAMA_STAGE_ANCIANA) ? WOOL_GREY : WOOL_CREAM;
    llama_model_configure(&g->model, st, sheared, true, wool);
}

/* ------------------------------------------------------- comportamiento */

static void set_action(llama_game *g, llama_anim a, float dur)
{
    g->action_anim  = a;
    g->action_timer = dur;
}

static void behaviour_update(llama_game *g, float dt)
{
    llama_model *m = &g->model;
    llama_pet   *p = &g->pet;

    if (llama_pet_dead(p)) {
        llama_model_set_anim(m, LA_DEAD);
        return;
    }
    if (g->action_timer > 0.f) {
        g->action_timer -= dt;
        llama_model_set_anim(m, g->action_anim);
        if (g->action_anim == LA_EAT && (llama_rand() & 15) == 0) {
            game_emit(g, PT_CRUMB, llama_model_mouth_pos(m), 1);
        }
        return;
    }
    if (llama_pet_sleeping(p)) {
        llama_model_set_anim(m, LA_SLEEP);
        m->heading = approach(m->heading, 0.35f, 1.2f, dt);
        if (llama_randf() < dt * 0.55f) {
            game_emit(g, PT_ZZZ, llama_model_head_pos(m), 1);
        }
        return;
    }

    /* Si hay comida servida, va al comedero. */
    if (g->bowl_timer > 0.f) {
        g->wander_target_x = 0.f;
        g->wander_target_z = 1.55f;
    }

    float dx = g->wander_target_x - m->pos.x;
    float dz = g->wander_target_z - m->pos.z;
    float dist = sqrtf(dx * dx + dz * dz);

    if (dist > 0.18f) {
        float speed = llama_pet_sick(p) ? 0.35f : 0.85f;
        m->pos.x += dx / dist * speed * dt;
        m->pos.z += dz / dist * speed * dt;
        float want = atan2f(dx, dz);
        float diff = want - m->heading;
        while (diff > (float)M_PI)  diff -= 2.f * (float)M_PI;
        while (diff < -(float)M_PI) diff += 2.f * (float)M_PI;
        m->heading += clampf(diff, -3.f * dt, 3.f * dt);
        llama_model_set_anim(m, llama_pet_sick(p) ? LA_SICK : LA_WALK);
        return;
    }

    /* Llego al comedero: come. */
    if (g->bowl_timer > 0.f) {
        set_action(g, LA_EAT, 3.2f);
        g->bowl_timer = 0.f;
        llama_plat_tone(660, 60);
        return;
    }

    if (llama_pet_sick(p)) {
        llama_model_set_anim(m, LA_SICK);
        return;
    }

    g->wander_timer -= dt;
    if (g->wander_timer <= 0.f) {
        g->wander_timer = 3.5f + llama_randf() * 6.f;
        if (llama_randf() < 0.35f) g->idle_side = -g->idle_side;
        if (p->energy > 25.f && p->happiness > 20.f && llama_randf() < 0.6f) {
            g->wander_target_x = (llama_randf() - 0.5f) * 3.2f;
            g->wander_target_z = (llama_randf() - 0.5f) * 2.6f;
        }
    }
    llama_model_set_anim(m, LA_IDLE);
    /* Se para de tres cuartos respecto de la camara: de frente el bicho se ve
     * como una astilla y se pierde la manta y la silueta. Cada tanto cambia de
     * lado para que no quede siempre igual. */
    float side = ((llama_rand() >> 7) & 1) ? 1.f : -1.f;
    if (g->idle_side == 0.f) g->idle_side = side;
    float want = g->cam_yaw + g->idle_side * 0.80f;
    float diff = want - m->heading;
    while (diff > (float)M_PI)  diff -= 2.f * (float)M_PI;
    while (diff < -(float)M_PI) diff += 2.f * (float)M_PI;
    m->heading += clampf(diff, -0.7f * dt, 0.7f * dt);
}

/* ---------------------------------------------------------------- acciones */

static void do_feed(llama_game *g, llama_food food)
{
    llama_result r = llama_pet_feed(&g->pet, food);
    switch (r) {
    case LLAMA_ACT_OK: {
        char msg[48];
        snprintf(msg, sizeof(msg), "%s en el comedero", llama_food_name(food));
        game_toast(g, msg);
        g->bowl_food  = (int)food;
        g->bowl_timer = 25.f;
        llama_plat_tone(880, 40);
        break;
    }
    case LLAMA_ACT_LLENA:
        game_toast(g, "¡No tiene hambre! Te escupió");
        set_action(g, LA_SPIT, 0.9f);
        game_emit(g, PT_SPIT, llama_model_mouth_pos(&g->model), 4);
        game_emit(g, PT_ANGRY, llama_model_head_pos(&g->model), 2);
        g->cam_shake = 0.5f;
        llama_plat_tone(220, 120);
        break;
    case LLAMA_ACT_SIN_MONEDAS: game_toast(g, "No te alcanzan las monedas"); break;
    case LLAMA_ACT_DURMIENDO:   game_toast(g, "Está durmiendo..."); break;
    default: break;
    }
}

static void do_clean(llama_game *g)
{
    llama_result r = llama_pet_clean(&g->pet);
    if (r == LLAMA_ACT_OK) {
        game_toast(g, "¡Corral limpio!");
        game_emit(g, PT_STAR, llama_model_body_pos(&g->model), 6);
        g->flash = 0.5f;
        llama_plat_tone(1200, 50);
    } else if (r == LLAMA_ACT_NO_HACE_FALTA) {
        game_toast(g, "Ya está limpio");
    }
}

static void do_sleep(llama_game *g)
{
    bool was = llama_pet_sleeping(&g->pet);
    llama_pet_sleep_toggle(&g->pet);
    if (!was && llama_pet_sleeping(&g->pet)) {
        game_toast(g, "Buenas noches...");
        llama_plat_tone(330, 120);
    } else {
        game_toast(g, "¡Buen día!");
        llama_plat_tone(770, 80);
    }
}

static void do_medicine(llama_game *g)
{
    llama_result r = llama_pet_medicine(&g->pet);
    switch (r) {
    case LLAMA_ACT_OK:
        game_toast(g, "Remedio tomado");
        game_emit(g, PT_STAR, llama_model_head_pos(&g->model), 5);
        llama_plat_tone(990, 60);
        break;
    case LLAMA_ACT_NO_HACE_FALTA: game_toast(g, "Está sana, no hace falta"); break;
    case LLAMA_ACT_SIN_MONEDAS:   game_toast(g, "Faltan monedas (cuesta 2)"); break;
    default: break;
    }
}

static void do_shear(llama_game *g)
{
    uint16_t before = g->pet.coins;
    llama_result r = llama_pet_shear(&g->pet);
    if (r == LLAMA_ACT_OK) {
        char msg[48];
        snprintf(msg, sizeof(msg), "Esquilada: +%d monedas", g->pet.coins - before);
        game_toast(g, msg);
        set_action(g, LA_SHEAR, 1.6f);
        game_emit(g, PT_COIN, llama_model_body_pos(&g->model), 5);
        sync_appearance(g);
        llama_plat_tone(1320, 70);
    } else if (r == LLAMA_ACT_NO_HACE_FALTA) {
        game_toast(g, "Todavía le falta lana");
    } else if (r == LLAMA_ACT_DURMIENDO) {
        game_toast(g, "Está durmiendo...");
    }
}

/* ------------------------------------------------------------------ toque */

static bool llama_hit(llama_game *g, g3d_target *t, int x, int y)
{
    float sx, sy, scale;
    g3d_v3 c = llama_model_body_pos(&g->model);
    if (!g3d_project(t, &g->ctx, c, &sx, &sy, &scale)) return false;
    float r = 0.75f * scale;
    float dx = x - sx, dy = y - sy;
    return (dx * dx + dy * dy) < r * r;
}

static void handle_tap_main(llama_game *g, g3d_target *t, int x, int y)
{
    int btn = ui_button_at(x, y);
    if (btn >= 0) {
        llama_plat_haptic(12);
        switch (btn) {
        case BTN_COMER:  g->screen = SCREEN_FOOD; g->screen_t = 0.f; break;
        case BTN_JUGAR:
            if (llama_pet_sleeping(&g->pet)) { game_toast(g, "Está durmiendo..."); break; }
            if (g->pet.energy < 12.f) { game_toast(g, "Está muy cansada"); break; }
            minigame_start(g);
            break;
        case BTN_ASEO:   do_clean(g); break;
        case BTN_DORMIR: do_sleep(g); break;
        case BTN_SALUD:  do_medicine(g); break;
        default: break;
        }
        return;
    }
    if (ui_shear_button_hit(g, x, y)) { do_shear(g); return; }
    if (y < HUD_H) { g->screen = SCREEN_STATS; g->screen_t = 0.f; return; }

    if (llama_hit(g, t, x, y)) {
        llama_result r = llama_pet_pet(&g->pet);
        if (r == LLAMA_ACT_OK) {
            game_emit(g, PT_HEART, llama_model_head_pos(&g->model), 2);
            if (llama_randf() < 0.25f) set_action(g, LA_HAPPY, 1.1f);
            llama_plat_tone(1046, 25);
        } else if (r == LLAMA_ACT_DURMIENDO) {
            game_emit(g, PT_ZZZ, llama_model_head_pos(&g->model), 1);
        }
    }
}

static void handle_input(llama_game *g, const llama_input *in, g3d_target *t, float dt)
{
    const bool down = in->touch_down;
    const uint32_t now = llama_plat_millis();

    if (down && !g->touch_prev) {
        g->press_x = g->last_x = in->touch_x;
        g->press_y = g->last_y = in->touch_y;
        g->press_ms = now;
        g->dragging = false;
        g->pressed_btn = ui_button_at(in->touch_x, in->touch_y);
    } else if (down && g->touch_prev) {
        int dx = in->touch_x - g->last_x;
        if (abs(in->touch_x - g->press_x) > 8 || abs(in->touch_y - g->press_y) > 10) {
            g->dragging = true;
        }
        if (g->dragging && g->pressed_btn < 0 && g->press_y < SCR_H - BAR_H) {
            g->cam_yaw_vel += (float)dx * 0.08f / (dt > 0.f ? 1.f : 1.f);
            g->cam_yaw -= (float)dx * 0.012f;
        }
        g->last_x = in->touch_x;
        g->last_y = in->touch_y;
    } else if (!down && g->touch_prev) {
        bool tap = !g->dragging && (now - g->press_ms) < 800;
        if (tap) {
            g->tap_pending = true;
            g->tap_x = g->press_x;
            g->tap_y = g->press_y;
        }
        g->pressed_btn = -1;
    }
    g->touch_prev = down;

    if (!g->tap_pending) return;
    g->tap_pending = false;
    const int x = g->tap_x, y = g->tap_y;

    switch (g->screen) {
    case SCREEN_BOOT:
        g->screen = llama_pet_dead(&g->pet) ? SCREEN_DEAD : SCREEN_MAIN;
        g->screen_t = 0.f;
        break;
    case SCREEN_MAIN:
        handle_tap_main(g, t, x, y);
        break;
    case SCREEN_FOOD: {
        /* Tres tarjetas apiladas + cerrar. */
        if (y > 96 && y < 232) {
            int idx = (y - 96) / 46;
            if (idx >= 0 && idx < LLAMA_FOOD_COUNT) {
                do_feed(g, (llama_food)idx);
                g->screen = SCREEN_MAIN;
            }
        } else {
            g->screen = SCREEN_MAIN;
        }
        g->screen_t = 0.f;
        break;
    }
    case SCREEN_STATS:
        g->screen = SCREEN_MAIN;
        g->screen_t = 0.f;
        break;
    case SCREEN_MINIGAME:
        if (x > SCR_W - 46 && y < 30) {
            g->mg.running = false;
            g->mg.end_timer = 0.01f;
        } else {
            minigame_update(g, 0.f, true);
        }
        break;
    case SCREEN_DEAD:
        if (y > 220) {
            llama_pet_new(&g->pet, llama_plat_time(),
                          NAMES[llama_rand() % (sizeof(NAMES) / sizeof(NAMES[0]))]);
            sync_appearance(g);
            g->screen = SCREEN_NEWBORN;
            g->screen_t = 0.f;
            llama_game_save(g);
        }
        break;
    case SCREEN_NEWBORN:
        if (y > 230 && x < SCR_W / 2) {
            strncpy(g->pet.name, NAMES[llama_rand() % (sizeof(NAMES) / sizeof(NAMES[0]))],
                    sizeof(g->pet.name) - 1);
        } else {
            g->screen = SCREEN_MAIN;
            g->screen_t = 0.f;
            llama_game_save(g);
        }
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------------- IMU */

static void handle_imu(llama_game *g, const llama_input *in, float dt)
{
    /*
     * Inclinacion para el parallax de camara. Se mide contra una linea de base
     * lenta, no contra el crudo del acelerometro: apoyada sobre la mesa la placa
     * lee ay ~= -1 por la gravedad, y tomando eso como inclinacion la camara se
     * iba agachando sola hasta saturar. Con la linea de base, cualquier postura
     * en reposo es "sin inclinacion" y solo el movimiento mueve la camara.
     */
    const float base_k = 1.f - expf(-dt / 2.5f);    /* la base sigue de a poco */
    const float tilt_k = 1.f - expf(-dt / 0.18f);   /* la respuesta es rapida */
    g->base_ax += (in->ax - g->base_ax) * base_k;
    g->base_ay += (in->ay - g->base_ay) * base_k;
    float want_x = clampf(in->ax - g->base_ax, -1.f, 1.f);
    float want_y = clampf(in->ay - g->base_ay, -1.f, 1.f);
    g->tilt_x += (want_x - g->tilt_x) * tilt_k;
    g->tilt_y += (want_y - g->tilt_y) * tilt_k;

    float d = fabsf(in->ax - g->last_ax) + fabsf(in->ay - g->last_ay) +
              fabsf(in->az - g->last_az);
    g->last_ax = in->ax;
    g->last_ay = in->ay;
    g->last_az = in->az;

    g->shake_energy = g->shake_energy * (1.f - 3.f * dt) + d;
    if (g->shake_energy > 2.2f) {
        g->shake_energy = 0.f;
        if (g->screen == SCREEN_MAIN && !llama_pet_dead(&g->pet)) {
            if (llama_pet_sleeping(&g->pet)) {
                llama_pet_sleep_toggle(&g->pet);
                game_toast(g, "¡La despertaste!");
                g->pet.happiness = clampf(g->pet.happiness - 4.f, 0.f, 100.f);
            } else if (g->action_timer <= 0.f) {
                set_action(g, LA_HAPPY, 1.4f);
                llama_pet_pet(&g->pet);
                game_emit(g, PT_STAR, llama_model_head_pos(&g->model), 4);
                game_toast(g, "¡Salta de alegría!");
                llama_plat_tone(1318, 40);
            }
        }
        g->cam_shake = 0.6f;
    }
}

/* ------------------------------------------------------------------ render */

/* Fila de pantalla donde cae el horizonte: es donde se proyecta un punto
 * infinitamente lejano a la altura del ojo. */
static int horizon_row(llama_game *g, g3d_target *t)
{
    g3d_v3 eye = g->ctx.eye;
    g3d_v3 fwd = g3d_v3_sub(g3d_v(g->model.pos.x * 0.5f, g->cam_target_y,
                                  g->model.pos.z * 0.5f), eye);
    fwd.y = 0.f;
    fwd = g3d_v3_norm(fwd);
    g3d_v3 far_pt = g3d_v3_add(eye, g3d_v3_mul(fwd, 4000.f));
    far_pt.y = eye.y;
    float sy = (float)(t->h / 2);
    g3d_project(t, &g->ctx, far_pt, NULL, &sy, NULL);
    return (int)(sy + 0.5f);
}

static void draw_sky(llama_game *g, g3d_target *t)
{
    if (g->use_pano) {
        /* El paisaje lejano ya trae cielo y cordillera; el color de ambiente le
         * pone la hora del dia. */
        g3d_pano_draw(t, &g->pano, (int)g->amb.season, g->cam_yaw,
                      horizon_row(g, t), g->amb.shade, g->amb.tint, g->amb.tint_a);
    } else {
        g3d_sky_gradient(t, g->amb.sky_top, g->amb.sky_bottom, 0, t->h);
    }

    if (g->use_ground) {
        /* El piso empieza solo en su horizonte geometrico; el tinte va mas
         * suave que en el panorama, igual que el resto del mundo 3D. */
        g3d_ground_draw(t, &g->ctx, &g->ground, (int)g->amb.season, 0,
                        g->amb.shade, g->ctx.light.tint_color, g->ctx.light.tint_a);
    }

    /* Con lluvia el cielo esta cubierto: el sol y las estrellas se apagan. */
    const float clear = 1.f - g->rain;
    if (clear <= 0.05f) return;
    const uint8_t ca = (uint8_t)(clear * 255.f);

    if (g->night > 0.5f) {
        /* Estrellas deterministas. */
        uint32_t s = 12345;
        for (int i = 0; i < 26; i++) {
            s = s * 1103515245u + 12345u;
            int x = (int)((s >> 16) % (uint32_t)t->w);
            s = s * 1103515245u + 12345u;
            int y = (int)((s >> 16) % 110u);
            g2d_blend_rect(t, x, y, 1, 1, g3d_rgb(240, 240, 255), ca);
        }
        g2d_blend_ellipse(t, 188, 40, 14, 14, g3d_rgb(238, 238, 220), ca);
        g2d_blend_ellipse(t, 182, 36, 12, 12,
                          g3d_color_lerp(SKY_NIGHT_TOP, SKY_NIGHT_BOT, 0.35f), ca);
    } else {
        /* Sol con halo en tres capas. */
        g2d_blend_ellipse(t, 196, 40, 34, 34, g3d_rgb(255, 240, 170), (uint8_t)(55 * clear));
        g2d_blend_ellipse(t, 196, 40, 22, 22, g3d_rgb(255, 244, 190), (uint8_t)(110 * clear));
        g2d_blend_ellipse(t, 196, 40, 14, 14, g3d_rgb(255, 246, 186), ca);
        g2d_blend_ellipse(t, 196, 40, 11, 11, g3d_rgb(255, 252, 224), ca);
    }
}

/*
 * Dibuja la llama: si hay hoja de sprites usa el modelo original renderizado
 * (ver g3d_sprite.h); si no, cae en la geometria procedural.
 */
void game_draw_llama(llama_game *g, g3d_target *t, float tint)
{
    if (!g->use_sprites) {
        llama_model_draw(t, &g->ctx, &g->model, tint);
        return;
    }
    const llama_model *m = &g->model;

    /* El rebote de la animacion mueve el sprite entero; no dejamos que lo
     * hunda bajo el piso cuando la pose es de descanso. */
    float bob = m->body_y > 0.f ? m->body_y : 0.f;
    g3d_v3 base = g3d_v(m->pos.x, m->pos.y + bob, m->pos.z);

    float sx, sy, pscale;
    if (!g3d_project(t, &g->ctx, base, &sx, &sy, &pscale)) return;
    if (g->sprite_ref_scale <= 0.f) return;

    /* Angulo aparente = orientacion de la llama menos el azimut de la camara. */
    int angle = g3d_sprite_angle_index(&g->sprites, m->heading - g->cam_yaw);
    float scale = m->scale * pscale / g->sprite_ref_scale;

    /*
     * Animacion de titere: el sprite es una sola pose, asi que toda la vida
     * sale de aplastarlo, estirarlo e inclinarlo. La escala vertical pivota en
     * los pies y la cizalla mueve el lomo, y con eso el trote, el mordisco y
     * el suenio se leen aunque la foto sea siempre la misma.
     */
    float sy_s = 1.f, sx_s = 1.f, shear = 0.f;
    switch (m->anim) {
    case LA_IDLE:
        sy_s = 1.f + 0.013f * sinf(m->t * 1.7f);            /* respira */
        break;
    case LA_WALK: {
        float ph = m->anim_t * 8.5f;
        sy_s = 1.f + 0.045f * sinf(ph);
        sx_s = 1.f - 0.030f * sinf(ph);
        shear = 0.035f * sinf(ph * 0.5f);                    /* balanceo del paso */
        break;
    }
    case LA_EAT: {
        /* Se agacha al comedero y pega mordiscos ritmicos. */
        float bite = 0.5f - 0.5f * cosf(m->anim_t * 6.2f);
        sy_s = 0.94f - 0.22f * bite;
        sx_s = 1.f + 0.07f * bite;
        float bsx;
        if (g3d_project(t, &g->ctx, g3d_v(0.f, 0.f, 2.05f), &bsx, NULL, NULL)) {
            shear = (bsx > sx ? 1.f : -1.f) * 0.10f * bite;  /* hacia el comedero */
        }
        break;
    }
    case LA_SLEEP:
        sy_s = 0.60f + 0.018f * sinf(m->t * 1.3f);           /* echada, respirando */
        sx_s = 1.14f;
        break;
    case LA_HAPPY: {
        float ph = m->anim_t * 8.f;
        float hop = sinf(ph) > 0.f ? sinf(ph) : 0.f;
        sy_s = 1.f + 0.14f * hop - 0.06f * (hop <= 0.01f);   /* estira al saltar */
        sx_s = 1.f - 0.08f * hop + 0.05f * (hop <= 0.01f);   /* aplasta al caer */
        break;
    }
    case LA_SICK:
        sy_s = 0.93f + 0.01f * sinf(m->t * 1.1f);
        shear = 0.045f;                                      /* caida, sin fuerza */
        break;
    case LA_SPIT: {
        float ph = m->anim_t * 9.f;
        shear = ph < 1.6f ? -0.09f * sinf(ph * 1.9f) : 0.14f * sinf((ph - 1.6f) * 2.6f);
        sy_s = 1.f + 0.05f * sinf(ph * 2.f);
        break;
    }
    case LA_SHEAR:
        shear = 0.05f * sinf(m->anim_t * 16.f);              /* se sacude */
        sy_s = 1.f + 0.02f * sinf(m->anim_t * 16.f);
        break;
    case LA_DEAD:
        sy_s = 0.42f;
        sx_s = 1.28f;
        break;
    default:
        break;
    }

    /* Profundidad del plano que pasa por el centro del bicho. */
    float invw = pscale / (g->sprite_ref_scale * g->cam_dist);
    /* scale_y es relativa a la escala en X, asi que se compensa sx_s. */
    g3d_sprite_draw(t, &g->sprites, angle, 0, sx, sy, scale * sx_s, sy_s / sx_s,
                    shear, invw, tint, g->ctx.light.tint_color, g->ctx.light.tint_a);
}

/*
 * Burbuja de pensamiento sobre la cabeza: muestra la necesidad mas urgente,
 * asi se entiende que pedirle al bicho sin mirar las barras. Parpadea para no
 * quedarse pegada a la escena.
 */
static void draw_need_bubble(llama_game *g, g3d_target *t)
{
    const llama_pet *p = &g->pet;
    if (llama_pet_dead(p) || llama_pet_sleeping(p) || g->action_timer > 0.f) return;

    int icon = -1;
    g3d_color col = g3d_rgb(90, 80, 70);
    if      (llama_pet_sick(p))     { icon = ICON_HEALTH; col = g3d_rgb(216, 76, 66); }
    else if (p->hunger < 38.f)      { icon = ICON_FOOD;   col = g3d_rgb(196, 148, 60); }
    else if (p->energy < 30.f)      { icon = ICON_MOON;   col = g3d_rgb(96, 92, 160); }
    else if (p->hygiene < 35.f)     { icon = ICON_CLEAN;  col = g3d_rgb(72, 138, 208); }
    else if (p->happiness < 30.f)   { icon = ICON_BALL;   col = g3d_rgb(222, 96, 120); }
    if (icon < 0) return;

    if (fmodf(g->model.t, 3.6f) > 2.55f) return;   /* respiro entre parpadeos */

    float hx, hy, hs;
    if (!g3d_project(t, &g->ctx, llama_model_head_pos(&g->model), &hx, &hy, &hs)) return;
    int bx = (int)hx + 16, by = (int)hy - 22;
    if (bx < 16) bx = 16;
    if (bx > t->w - 18) bx = t->w - 18;
    if (by < 52) by = 52;

    const g3d_color blanco = g3d_rgb(252, 250, 244);
    g2d_fill_circle(t, bx - 11, by + 12, 2, blanco);   /* colita de la burbuja */
    g2d_fill_circle(t, bx - 7, by + 8, 3, blanco);
    g2d_blend_ellipse(t, bx, by, 13, 11, blanco, 235);
    ui_draw_icon(t, icon, bx, by, 8, col);
}

static void draw_world_and_pet(llama_game *g, g3d_target *t)
{
    const float night = g->night;
    llama_scene_draw_world(t, &g->ctx, &g->scene, night);

    /* Sombra: entre el piso y la llama. */
    g3d_v3 sp = g3d_v(g->model.pos.x, 0.02f, g->model.pos.z);
    llama_scene_shadow(t, &g->ctx, sp, 0.55f * g->model.scale, 110);

    llama_scene_draw_poops(t, &g->ctx, &g->scene, g->pet.poops, night);

    if (g->bowl_timer > 0.f || g->action_anim == LA_EAT) {
        g3d_v3 bp = g3d_v(0.f, 0.f, 2.05f);
        llama_scene_draw_mesh_at(t, &g->ctx, &g->scene.bowl, bp, 0.f, 1.f, 1.f - night * 0.4f);
        const g3d_mesh *food = (g->bowl_food == LLAMA_FOOD_MANZANA) ? &g->scene.apple
                                                                    : &g->scene.hay;
        llama_scene_draw_mesh_at(t, &g->ctx, food, g3d_v(bp.x, 0.14f, bp.z), 0.f, 1.f,
                                 1.f - night * 0.4f);
    }

    if (llama_pet_dead(&g->pet)) {
        llama_scene_draw_mesh_at(t, &g->ctx, &g->scene.grave, g3d_v(1.5f, 0.f, 0.4f),
                                 0.2f, 1.f, 1.f - night * 0.4f);
    }

    float tint = 1.f - night * 0.35f;
    if (llama_pet_sick(&g->pet)) tint *= 0.85f;
    game_draw_llama(g, t, tint);
    draw_need_bubble(g, t);
    game_particles_draw(g, t);
}

/* Viñeteado: unos pocos marcos concentricos alcanzan y salen casi gratis. */
static void draw_vignette(g3d_target *t)
{
    for (int i = 0; i < 14; i++) {
        uint8_t a = (uint8_t)(26 - i * 2);
        if (a == 0) break;
        int x = i * 2, y = i * 2;
        g2d_blend_rect(t, x, y, t->w - 2 * x, 2, g3d_rgb(0, 0, 0), a);
        g2d_blend_rect(t, x, t->h - y - 2, t->w - 2 * x, 2, g3d_rgb(0, 0, 0), a);
        g2d_blend_rect(t, x, y, 2, t->h - 2 * y, g3d_rgb(0, 0, 0), a);
        g2d_blend_rect(t, t->w - x - 2, y, 2, t->h - 2 * y, g3d_rgb(0, 0, 0), a);
    }
}

/* -------------------------------------------------------------- ciclo vida */

llama_game *llama_game_create(int w, int h)
{
    llama_game *g = (llama_game *)calloc(1, sizeof(llama_game));
    if (!g) return NULL;
    g->w = w;
    g->h = h;

    llama_rand_seed(llama_plat_seed());

    if (!llama_model_init(&g->model) || !llama_scene_init(&g->scene)) {
        llama_game_destroy(g);
        return NULL;
    }

    double now = llama_plat_time();
    bool loaded = llama_plat_load(&g->pet, sizeof(g->pet));
    if (!loaded || g->pet.magic != LLAMA_SAVE_MAGIC || g->pet.version != LLAMA_SAVE_VERSION) {
        llama_pet_new(&g->pet, now, NAMES[llama_rand() % (sizeof(NAMES) / sizeof(NAMES[0]))]);
        g->screen = SCREEN_NEWBORN;
    } else {
        llama_pet_catch_up(&g->pet, now);
        g->screen = SCREEN_BOOT;
    }

    g->ctx.light.light_dir = g3d_v3_norm(g3d_v(-0.42f, 0.80f, 0.42f));
    g->ctx.light.ambient   = 0.56f;
    g->ctx.light.diffuse   = 0.56f;
    g->ctx.light.rim       = 0.10f;
    g->ctx.light.fog_color = SKY_DAY_BOT;
    g->ctx.light.fog_start = 11.f;
    g->ctx.light.fog_end   = 62.f;

    /* Hoja de sprites del modelo original. La escala de referencia se mide con
     * la misma camara con la que los renderizo la herramienta, asi que el
     * sprite queda del tamano exacto que tendria el modelo en 3D. */
    size_t sprite_len = 0;
    const void *sprite_blob = llama_plat_sprites(&sprite_len);
    if (sprite_blob && g3d_sprite_open(&g->sprites, sprite_blob, sprite_len)) {
        g->use_sprites = true;
    }

    size_t pano_len = 0;
    const void *pano_blob = llama_plat_pano(&pano_len);
    if (pano_blob && g3d_pano_open(&g->pano, pano_blob, pano_len)) {
        g->use_pano = true;
        /* Con panorama, la cordillera y las nubes 3D estarian de mas. */
        g->scene.backdrop_3d = false;
    }

    size_t ground_len = 0;
    const void *ground_blob = llama_plat_ground(&ground_len);
    if (ground_blob && g3d_ground_open(&g->ground, ground_blob, ground_len)) {
        g->use_ground = true;
        /* Con piso texturizado, la malla de anillos queda de mas. */
        g->scene.ground_3d = false;
    }

    g->cam_yaw      = 0.f;
    g->cam_dist     = 4.55f;
    g->cam_height   = 1.62f;
    g->cam_target_y = 0.88f;
    g->pressed_btn  = -1;
    g->last_time    = now;
    g->battery_pct  = -1;
    g->wander_timer = 2.f;

    sync_appearance(g);
    llama_model_update(&g->model, 0.f);

    if (g->use_sprites) {
        /* Camara canonica (llama en el origen, cam_yaw = 0) para medir cuantos
         * pixeles por unidad de mundo uso la herramienta. */
        g3d_ctx ref = g->ctx;
        g3d_ctx_camera(&ref, g3d_v(0.f, g->cam_height, g->cam_dist),
                       g3d_v(0.f, g->cam_target_y, 0.f), 0.72f,
                       (float)w / (float)h, 0.15f, 60.f);
        g3d_target probe = { .w = w, .h = h, .color = NULL, .depth = NULL };
        float sc = 1.f;
        g3d_project(&probe, &ref, g3d_v(0.f, 0.f, 0.f), NULL, NULL, &sc);
        g->sprite_ref_scale = sc;
    }
    return g;
}

void llama_game_destroy(llama_game *g)
{
    if (!g) return;
    llama_model_free(&g->model);
    llama_scene_free(&g->scene);
    free(g);
}

void llama_game_save(llama_game *g)
{
    if (!g) return;
    g->pet.last_seen = llama_plat_time();
    llama_plat_save(&g->pet, sizeof(g->pet));
    g->save_timer = 0.f;
}

const llama_pet *llama_game_pet(const llama_game *g) { return &g->pet; }

bool llama_game_busy(const llama_game *g)
{
    return g->action_timer > 0.f || g->toast_t > 0.f || g->screen == SCREEN_MINIGAME ||
           g->cam_yaw_vel != 0.f;
}

/* ------------------------------------------------------------------- frame */

void llama_game_frame(llama_game *g, const llama_input *in, float dt, g3d_target *fb)
{
    if (dt > 0.25f) dt = 0.25f;   /* evita saltos tras una pausa larga */

    g->screen_t += dt;
    g->battery_pct = in->battery_pct;
    g->charging = in->charging;

    /* Estacion y hora del dia, del reloj de tiempo real. */
    double now = llama_plat_time();
    llama_ambient_eval(&g->amb, now);
    llama_scene_set_season(&g->scene, g->amb.season);
    weather_update(g, dt);

    /* Nublado: la lluvia apaga el sol y lleva todo hacia un gris azulado. Se
     * corrige el ambiente de la hora antes de repartirlo al fondo y a la luz. */
    if (g->rain > 0.01f) {
        const g3d_color grey = g3d_rgb(96, 104, 118);
        const float r = g->rain;
        g->amb.shade *= 1.f - 0.30f * r;
        g->amb.tint   = g3d_color_lerp(g->amb.tint, grey, r * 0.75f);
        int a = (int)g->amb.tint_a + (int)(120.f * r);
        g->amb.tint_a = (uint8_t)(a > 200 ? 200 : a);
        g->amb.fog    = g3d_color_lerp(g->amb.fog, grey, r * 0.55f);
    }

    g->night = approach(g->night, g->amb.night, 0.8f, dt);
    g->ctx.light.fog_color = g->amb.fog;
    /* El color de la hora tambien va al mundo 3D, pero mas suave que en el
     * panorama: el fondo se puede fundir en la noche, la llama tiene que
     * seguir leyendose. */
    g->ctx.light.tint_color = g->amb.tint;
    g->ctx.light.tint_a     = (uint8_t)((int)g->amb.tint_a * 5 / 8);

    handle_input(g, in, fb, dt);
    handle_imu(g, in, dt);

    /* Simulacion de la mascota (siempre, incluso en el minijuego). */
    llama_pet_update(&g->pet, dt);
    sync_appearance(g);

    if (llama_pet_dead(&g->pet) && g->screen != SCREEN_DEAD && g->screen != SCREEN_NEWBORN) {
        g->screen = SCREEN_DEAD;
        g->screen_t = 0.f;
    }

    if (g->bowl_timer > 0.f) g->bowl_timer -= dt;
    if (g->toast_t > 0.f)    g->toast_t -= dt;
    if (g->flash > 0.f)      g->flash -= dt * 2.f;

    if (g->screen == SCREEN_MINIGAME) {
        minigame_update(g, dt, false);
    } else {
        behaviour_update(g, dt);
    }

    llama_model_update(&g->model, dt);
    game_particles_update(g, dt);
    if (g->screen != SCREEN_MINIGAME) update_camera(g, dt);

    /* ---- render ---- */
    g->ctx.tris_drawn = 0;
    g3d_clear_depth(fb);
    draw_sky(g, fb);

    if (g->screen == SCREEN_MINIGAME) {
        minigame_draw(g, fb);
        draw_vignette(fb);
        ui_draw_minigame(g, fb);
    } else {
        draw_world_and_pet(g, fb);
        draw_vignette(fb);
        if (g->flash > 0.f) {
            g2d_blend_rect(fb, 0, 0, fb->w, fb->h, g3d_rgb(255, 255, 255),
                           (uint8_t)(g->flash * 120.f));
        }
        switch (g->screen) {
        case SCREEN_BOOT:    ui_draw_boot(g, fb); break;
        case SCREEN_DEAD:    ui_draw_dead(g, fb); break;
        case SCREEN_STATS:   ui_draw_hud(g, fb); ui_draw_stats(g, fb); break;
        case SCREEN_FOOD:    ui_draw_hud(g, fb); ui_draw_food_menu(g, fb); break;
        case SCREEN_NEWBORN: ui_draw_boot(g, fb); break;
        default:
            ui_draw_hud(g, fb);
            ui_draw_buttons(g, fb);
            break;
        }
        ui_draw_toast(g, fb);
    }

    /* ---- guardado periodico ---- */
    g->save_timer += dt;
    if (g->save_timer > 30.f) llama_game_save(g);

    /* ---- fps ---- */
    g->fps_acc += dt;
    g->fps_frames++;
    if (g->fps_acc >= 1.f) {
        g->fps = g->fps_frames;
        g->fps_frames = 0;
        g->fps_acc = 0.f;
    }
}
