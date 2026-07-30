/*
 * Minijuego "Salto de la llama": la llama corre y hay que saltar las piedras.
 * Usa el mismo motor 3D, con la camara de costado.
 */
#include "game_internal.h"
#include "llama_platform.h"
#include <math.h>
#include <stdio.h>

#define MG_START_X   9.f
#define MG_END_X    -4.5f
#define MG_DURATION 45.f

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void minigame_start(llama_game *g)
{
    minigame_state *m = &g->mg;
    m->running = true;
    m->t = 0.f;
    m->speed = 3.4f;
    m->jump_y = 0.f;
    m->jump_v = 0.f;
    m->jumping = false;
    m->score = 0;
    m->lives = 3;
    m->hit_flash = 0.f;
    m->end_timer = 0.f;
    for (int i = 0; i < MG_OBSTACLES; i++) {
        m->obst_x[i] = MG_START_X + i * 4.6f;
        m->obst_alive[i] = true;
    }
    g->screen = SCREEN_MINIGAME;
    g->screen_t = 0.f;
    g->model.pos = g3d_v(0.f, 0.f, 0.f);
    g->model.heading = (float)M_PI * 0.5f;   /* mira hacia +X */
    llama_model_set_anim(&g->model, LA_WALK);
    llama_plat_tone(880, 60);
}

static void minigame_finish(llama_game *g)
{
    minigame_state *m = &g->mg;
    m->running = false;
    if (m->end_timer <= 0.f) m->end_timer = 2.4f;
}

void minigame_update(llama_game *g, float dt, bool tap)
{
    minigame_state *m = &g->mg;

    if (tap) {
        if (!m->running && m->end_timer > 0.f) {
            m->end_timer = 0.01f;   /* saltear la pantalla de fin */
            return;
        }
        if (!m->jumping) {
            m->jumping = true;
            m->jump_v = 4.6f;
            llama_plat_tone(1046, 35);
        }
        return;
    }

    if (!m->running) {
        m->end_timer -= dt;
        if (m->end_timer <= 0.f) {
            llama_pet_play(&g->pet, m->score);
            g->screen = SCREEN_MAIN;
            g->screen_t = 0.f;
            g->model.pos = g3d_v(0.f, 0.f, 0.f);
            g->model.heading = 0.f;
            g->wander_timer = 1.f;
            g->wander_target_x = 0.f;
            g->wander_target_z = 0.f;
            char msg[40];
            if (m->score > 0) snprintf(msg, sizeof(msg), "¡%d puntos! Se divirtió", m->score);
            else              snprintf(msg, sizeof(msg), "Otra vez será...");
            game_toast(g, msg);
        }
        return;
    }

    m->t += dt;
    m->speed += dt * 0.16f;
    if (m->hit_flash > 0.f) m->hit_flash -= dt * 3.f;

    /* Salto. */
    if (m->jumping) {
        m->jump_y += m->jump_v * dt;
        m->jump_v -= 11.5f * dt;
        if (m->jump_y <= 0.f) {
            m->jump_y = 0.f;
            m->jump_v = 0.f;
            m->jumping = false;
        }
    }
    g->model.pos.y = m->jump_y;

    /* Obstaculos. */
    for (int i = 0; i < MG_OBSTACLES; i++) {
        m->obst_x[i] -= m->speed * dt;
        if (m->obst_x[i] < MG_END_X) {
            m->obst_x[i] += MG_OBSTACLES * 4.6f + (float)(llama_rand() % 3) * 0.7f;
            m->obst_alive[i] = true;
            m->score++;
            if ((m->score % 5) == 0) llama_plat_tone(1318, 30);
        }
        if (m->obst_alive[i] && fabsf(m->obst_x[i]) < 0.55f && m->jump_y < 0.62f) {
            m->obst_alive[i] = false;
            m->lives--;
            m->hit_flash = 1.f;
            g->cam_shake = 1.f;
            game_emit(g, PT_ANGRY, llama_model_head_pos(&g->model), 3);
            llama_plat_tone(180, 140);
            llama_plat_haptic(40);
            if (m->lives <= 0) minigame_finish(g);
        }
    }

    if (m->t > MG_DURATION) minigame_finish(g);

    /* Animacion: corre, salvo cuando esta en el aire. */
    llama_model_set_anim(&g->model, m->jumping ? LA_HAPPY : LA_WALK);

    /* Camara lateral fija. */
    g3d_v3 eye = g3d_v(1.1f, 2.05f + m->jump_y * 0.22f + g->cam_shake * 0.06f, 7.4f);
    g3d_v3 tgt = g3d_v(0.9f, 0.95f + m->jump_y * 0.45f, 0.f);
    g3d_ctx_camera(&g->ctx, eye, tgt, 0.72f, (float)g->w / (float)g->h, 0.15f, 60.f);
}

void minigame_draw(llama_game *g, g3d_target *t)
{
    minigame_state *m = &g->mg;
    const float night = g->night;

    llama_scene_draw_world(t, &g->ctx, &g->scene, night);

    /* Piedras de fondo para dar sensacion de velocidad. */
    for (int i = 0; i < 5; i++) {
        float x = fmodf(m->t * m->speed * 0.7f + i * 2.3f, 12.f) - 6.f;
        llama_scene_draw_mesh_at(t, &g->ctx, &g->scene.rock,
                                 g3d_v(-x, 0.f, -2.6f - (i & 1) * 0.7f),
                                 (float)i, 0.55f, 0.85f);
    }

    llama_scene_shadow(t, &g->ctx, g3d_v(g->model.pos.x, 0.02f, g->model.pos.z),
                       0.5f * g->model.scale * (1.f - m->jump_y * 0.25f),
                       (uint8_t)(110 - m->jump_y * 40.f));

    for (int i = 0; i < MG_OBSTACLES; i++) {
        if (!m->obst_alive[i]) continue;
        llama_scene_draw_mesh_at(t, &g->ctx, &g->scene.rock,
                                 g3d_v(m->obst_x[i], 0.f, 0.f), (float)i * 0.9f, 1.f,
                                 1.f - night * 0.4f);
    }

    float tint = 1.f - night * 0.35f;
    if (m->hit_flash > 0.f) tint *= (1.f + m->hit_flash * 0.6f);
    game_draw_llama(g, t, tint);
    game_particles_draw(g, t);
}
