/*
 * Interfaz: HUD, botones, menus y carteles. Todo dibujado a mano con g2d
 * para no depender de LVGL ni de mapas de bits externos.
 */
#include "game_internal.h"
#include "llama_platform.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define BTN_W (SCR_W / BTN_COUNT)
#define BAR_Y (SCR_H - BAR_H)

#define C_INK        g3d_rgb( 34,  30,  44)
#define C_PANEL      g3d_rgb( 46,  42,  60)
#define C_PANEL_LT   g3d_rgb( 70,  64,  88)
#define C_WHITE      g3d_rgb(248, 248, 252)
#define C_CREAM      g3d_rgb(244, 232, 206)
#define C_GOOD       g3d_rgb( 92, 200, 108)
#define C_WARN       g3d_rgb(244, 196,  62)
#define C_BAD        g3d_rgb(232,  86,  76)
#define C_COIN       g3d_rgb(248, 206,  70)
#define C_ACCENT     g3d_rgb(236, 128,  92)

/* ------------------------------------------------------------------ iconos */

void ui_draw_icon(g3d_target *t, int kind, int cx, int cy, int s, g3d_color col)
{
    const int h = s / 2;
    switch (kind) {
    case ICON_FOOD: /* comedero con pastito */
        g2d_fill_tri(t, cx - h, cy - h / 2, cx + h, cy - h / 2, cx + h / 2, cy + h, col);
        g2d_fill_tri(t, cx - h, cy - h / 2, cx + h / 2, cy + h, cx - h / 2, cy + h, col);
        g2d_hline(t, cx - h - 1, cy - h / 2, s + 2, col);
        g2d_vline(t, cx, cy - h - 2, h, C_GOOD);
        break;
    case ICON_HAPPY: /* carita */
        g2d_circle(t, cx, cy, h, col);
        g2d_pixel(t, cx - h / 2, cy - h / 3, col);
        g2d_pixel(t, cx + h / 2, cy - h / 3, col);
        g2d_hline(t, cx - h / 2, cy + h / 3, h + 1, col);
        g2d_pixel(t, cx - h / 2 - 1, cy + h / 3 - 1, col);
        g2d_pixel(t, cx + h / 2 + 1, cy + h / 3 - 1, col);
        break;
    case ICON_ENERGY: /* rayo */
        g2d_fill_tri(t, cx + h / 2, cy - h, cx - h / 2, cy + 1, cx + 1, cy + 1, col);
        g2d_fill_tri(t, cx - h / 2, cy + h, cx + h / 2, cy - 1, cx - 1, cy - 1, col);
        break;
    case ICON_CLEAN: /* gota */
        g2d_fill_tri(t, cx, cy - h - 1, cx - h, cy + h / 2, cx + h, cy + h / 2, col);
        g2d_fill_circle(t, cx, cy + h / 2, h - 1 > 1 ? h - 1 : 1, col);
        break;
    case ICON_HEALTH: /* cruz */
        g2d_fill_rect(t, cx - h / 3, cy - h, (h * 2) / 3 + 1, s, col);
        g2d_fill_rect(t, cx - h, cy - h / 3, s, (h * 2) / 3 + 1, col);
        break;
    case ICON_MOON:
        g2d_fill_circle(t, cx, cy, h, col);
        g2d_fill_circle(t, cx + h / 2, cy - h / 3, h, C_PANEL);
        break;
    case ICON_SUN:
        g2d_fill_circle(t, cx, cy, h - 1, col);
        for (int i = 0; i < 8; i++) {
            float a = i * 0.7853f;
            g2d_pixel(t, cx + (int)(cosf(a) * (h + 1)), cy + (int)(sinf(a) * (h + 1)), col);
        }
        break;
    case ICON_BALL:
        g2d_fill_circle(t, cx, cy, h, col);
        g2d_hline(t, cx - h, cy, s + 1, g3d_rgb(250, 240, 200));
        g2d_vline(t, cx, cy - h, s + 1, g3d_rgb(250, 240, 200));
        break;
    case ICON_COIN:
        g2d_fill_circle(t, cx, cy, h, C_COIN);
        g2d_circle(t, cx, cy, h, g3d_rgb(186, 142, 32));
        break;
    case ICON_SCISSORS:
        g2d_line(t, cx - h, cy - h, cx + h, cy + h - 2, col);
        g2d_line(t, cx + h, cy - h, cx - h, cy + h - 2, col);
        g2d_circle(t, cx - h + 1, cy + h - 1, 2, col);
        g2d_circle(t, cx + h - 1, cy + h - 1, 2, col);
        break;
    case ICON_WOOL:
        g2d_fill_circle(t, cx - h / 2, cy, h - 1, col);
        g2d_fill_circle(t, cx + h / 2, cy - 1, h - 1, col);
        g2d_fill_circle(t, cx, cy + h / 2, h - 1, col);
        break;
    default:
        break;
    }
}

/* -------------------------------------------------------------- geometria */

int ui_button_at(int x, int y)
{
    if (y < BAR_Y || y >= SCR_H) return -1;
    int i = x / BTN_W;
    if (i < 0) i = 0;
    if (i >= BTN_COUNT) i = BTN_COUNT - 1;
    return i;
}

#define SHEAR_CX 216
#define SHEAR_CY 74
#define SHEAR_R  17

bool ui_shear_button_hit(const llama_game *g, int x, int y)
{
    if (g->pet.wool < 60.f || llama_pet_dead(&g->pet)) return false;
    int dx = x - SHEAR_CX, dy = y - SHEAR_CY;
    return dx * dx + dy * dy < (SHEAR_R + 6) * (SHEAR_R + 6);
}

/* -------------------------------------------------------------------- HUD */

static g3d_color bar_color(float v)
{
    if (v > 60.f) return C_GOOD;
    if (v > 30.f) return C_WARN;
    return C_BAD;
}

static void draw_bar(g3d_target *t, int x, int y, int w, int h, float v, g3d_color col)
{
    g2d_fill_round_rect(t, x, y, w, h, h / 2, g3d_rgb(30, 28, 40));
    int fill = (int)((w - 2) * (v / 100.f));
    if (fill < 2 && v > 0.f) fill = 2;
    if (fill > 0) g2d_fill_round_rect(t, x + 1, y + 1, fill, h - 2, (h - 2) / 2, col);
}

static void draw_battery(g3d_target *t, int x, int y, int pct, bool charging)
{
    g2d_round_rect(t, x, y, 18, 9, 2, C_WHITE);
    g2d_fill_rect(t, x + 18, y + 3, 2, 3, C_WHITE);
    if (pct >= 0) {
        int w = (pct * 14) / 100;
        g3d_color c = pct > 40 ? C_GOOD : (pct > 15 ? C_WARN : C_BAD);
        if (w > 0) g2d_fill_rect(t, x + 2, y + 2, w, 5, c);
    }
    if (charging) {
        g2d_line(t, x + 11, y + 1, x + 7, y + 5, C_COIN);
        g2d_line(t, x + 9, y + 4, x + 6, y + 8, C_COIN);
    }
}

void ui_draw_hud(llama_game *g, g3d_target *t)
{
    const llama_pet *p = &g->pet;
    g2d_blend_rect(t, 0, 0, SCR_W, HUD_H, C_INK, 172);
    g2d_hline(t, 0, HUD_H - 1, SCR_W, C_PANEL_LT);

    char line[40];
    snprintf(line, sizeof(line), "%s", p->name);
    g2d_text(t, 5, 4, 1, C_CREAM, line);
    int nw = g2d_text_width(1, line);
    snprintf(line, sizeof(line), "%s", llama_stage_name(llama_pet_stage(p)));
    g2d_text(t, 5 + nw + 6, 4, 1, g3d_rgb(168, 160, 190), line);

    /* Monedas + bateria. */
    draw_battery(t, SCR_W - 24, 3, g->battery_pct, g->charging);
    snprintf(line, sizeof(line), "%u", (unsigned)p->coins);
    int cw = g2d_text_width(1, line);
    ui_draw_icon(t, ICON_COIN, SCR_W - 34 - cw - 6, 7, 8, C_COIN);
    g2d_text(t, SCR_W - 34 - cw, 4, 1, C_COIN, line);

    /* Cuatro medidores. */
    struct { int icon; float v; } bars[4] = {
        { ICON_FOOD,   p->hunger },
        { ICON_HAPPY,  p->happiness },
        { ICON_ENERGY, p->energy },
        { ICON_CLEAN,  p->hygiene },
    };
    for (int i = 0; i < 4; i++) {
        int col = i % 2, row = i / 2;
        int x = 5 + col * 118;
        int y = 16 + row * 13;
        ui_draw_icon(t, bars[i].icon, x + 4, y + 4, 9, C_WHITE);
        draw_bar(t, x + 11, y, 100, 9, bars[i].v, bar_color(bars[i].v));
    }

    /* Avisos. */
    if (llama_pet_sick(p)) {
        ui_draw_icon(t, ICON_HEALTH, SCR_W - 52, 8, 10, C_BAD);
    }
    if (g->pet.wool >= 60.f && !llama_pet_dead(&g->pet)) {
        float pulse = 0.5f + 0.5f * sinf(g->model.t * 3.f);
        g2d_fill_circle(t, SHEAR_CX, SHEAR_CY, SHEAR_R, g3d_color_shade(C_ACCENT, 0.7f + pulse * 0.3f));
        g2d_circle(t, SHEAR_CX, SHEAR_CY, SHEAR_R, C_CREAM);
        ui_draw_icon(t, ICON_SCISSORS, SHEAR_CX, SHEAR_CY - 2, 14, C_WHITE);
        g2d_text_center(t, SHEAR_CX, SHEAR_CY + 8, 1, C_WHITE, "lana");
    }
}

/* ---------------------------------------------------------------- botones */

void ui_draw_buttons(llama_game *g, g3d_target *t)
{
    static const char *labels[BTN_COUNT] = { "Comer", "Jugar", "Aseo", "Dormir", "Salud" };
    const llama_pet *p = &g->pet;

    g2d_blend_rect(t, 0, BAR_Y, SCR_W, BAR_H, C_INK, 196);
    g2d_hline(t, 0, BAR_Y, SCR_W, C_PANEL_LT);

    for (int i = 0; i < BTN_COUNT; i++) {
        int x = i * BTN_W;
        bool pressed = (g->pressed_btn == i);
        if (pressed) g2d_fill_rect(t, x + 2, BAR_Y + 2, BTN_W - 4, BAR_H - 4, C_PANEL_LT);
        if (i > 0) g2d_vline(t, x, BAR_Y + 6, BAR_H - 12, C_PANEL);

        int cx = x + BTN_W / 2;
        int cy = BAR_Y + 17;
        g3d_color col = C_WHITE;
        int icon;
        switch (i) {
        case BTN_COMER:  icon = ICON_FOOD;   col = C_CREAM; break;
        case BTN_JUGAR:  icon = ICON_BALL;   col = g3d_rgb(236, 116, 140); break;
        case BTN_ASEO:   icon = ICON_CLEAN;  col = g3d_rgb(120, 190, 240); break;
        case BTN_DORMIR: icon = llama_pet_sleeping(p) ? ICON_SUN : ICON_MOON;
                         col = llama_pet_sleeping(p) ? C_COIN : g3d_rgb(190, 190, 240); break;
        default:         icon = ICON_HEALTH; col = llama_pet_sick(p) ? C_BAD : C_WHITE; break;
        }
        ui_draw_icon(t, icon, cx, cy, 16, col);
        const char *lbl = (i == BTN_DORMIR && llama_pet_sleeping(p)) ? "Levantar" : labels[i];
        g2d_text_center(t, cx, BAR_Y + 31, 1, C_WHITE, lbl);

        /* Punto rojo de aviso. */
        bool alert = (i == BTN_COMER  && p->hunger  < 30.f) ||
                     (i == BTN_JUGAR  && p->happiness < 30.f) ||
                     (i == BTN_ASEO   && (p->poops >= 2 || p->hygiene < 30.f)) ||
                     (i == BTN_DORMIR && p->energy < 20.f && !llama_pet_sleeping(p)) ||
                     (i == BTN_SALUD  && llama_pet_sick(p));
        if (alert && fmodf(g->model.t, 1.0f) < 0.6f) {
            g2d_fill_circle(t, x + BTN_W - 9, BAR_Y + 9, 3, C_BAD);
        }
    }
}

/* ------------------------------------------------------------- menu comida */

void ui_draw_food_menu(llama_game *g, g3d_target *t)
{
    g2d_blend_rect(t, 0, 0, SCR_W, SCR_H, C_INK, 150);
    g2d_fill_round_rect(t, 14, 62, SCR_W - 28, 182, 10, C_PANEL);
    g2d_round_rect(t, 14, 62, SCR_W - 28, 182, 10, C_PANEL_LT);
    g2d_text_center(t, SCR_W / 2, 72, 1, C_CREAM, "¿Qué le damos de comer?");

    static const char *desc[LLAMA_FOOD_COUNT] = {
        "+18 hambre", "+34 hambre", "+14 y mucha alegría"
    };
    for (int i = 0; i < LLAMA_FOOD_COUNT; i++) {
        int y = 96 + i * 46;
        bool afford = g->pet.coins >= llama_food_price((llama_food)i);
        g2d_fill_round_rect(t, 22, y, SCR_W - 44, 40, 8,
                            afford ? C_PANEL_LT : g3d_rgb(56, 52, 66));
        ui_draw_icon(t, i == LLAMA_FOOD_MANZANA ? ICON_HAPPY : ICON_FOOD, 44, y + 20, 16,
                     afford ? C_CREAM : g3d_rgb(120, 116, 130));
        g2d_text(t, 64, y + 8, 1, afford ? C_WHITE : g3d_rgb(140, 136, 150),
                 llama_food_name((llama_food)i));
        g2d_text(t, 64, y + 22, 1, g3d_rgb(180, 176, 196), desc[i]);

        char price[16];
        int pr = llama_food_price((llama_food)i);
        if (pr == 0) snprintf(price, sizeof(price), "gratis");
        else         snprintf(price, sizeof(price), "%d", pr);
        int pw = g2d_text_width(1, price);
        int px = SCR_W - 36 - pw - (pr > 0 ? 11 : 0);
        g2d_text(t, px, y + 15, 1, pr == 0 ? C_GOOD : C_COIN, price);
        if (pr > 0) ui_draw_icon(t, ICON_COIN, px + pw + 8, y + 19, 9, C_COIN);
    }
    g2d_text_center(t, SCR_W / 2, 232, 1, g3d_rgb(160, 156, 176), "tocá afuera para cerrar");
}

/* ------------------------------------------------------------- estadisticas */

static void fmt_age(uint32_t s, char *out, size_t n)
{
    uint32_t d = s / 86400;
    uint32_t h = (s % 86400) / 3600;
    uint32_t m = (s % 3600) / 60;
    if (d > 0)      snprintf(out, n, "%ud %uh", (unsigned)d, (unsigned)h);
    else if (h > 0) snprintf(out, n, "%uh %um", (unsigned)h, (unsigned)m);
    else            snprintf(out, n, "%um", (unsigned)m);
}

void ui_draw_stats(llama_game *g, g3d_target *t)
{
    const llama_pet *p = &g->pet;
    g2d_blend_rect(t, 0, 0, SCR_W, SCR_H, C_INK, 165);
    g2d_fill_round_rect(t, 12, 52, SCR_W - 24, 200, 10, C_PANEL);
    g2d_round_rect(t, 12, 52, SCR_W - 24, 200, 10, C_PANEL_LT);
    g2d_text_center(t, SCR_W / 2, 60, 1, C_CREAM, "FICHA DE LA LLAMA");
    g2d_hline(t, 24, 72, SCR_W - 48, C_PANEL_LT);

    char v[32], age[24];
    fmt_age(p->age_s, age, sizeof(age));
    struct { const char *k; const char *v; } rows[10];
    char b[10][32];
    int n = 0;

    snprintf(b[n], 32, "%s", p->name);              rows[n].k = "Nombre";   rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%s", llama_stage_name(llama_pet_stage(p)));
                                                    rows[n].k = "Etapa";    rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%s", age);                  rows[n].k = "Edad";     rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%.1f kg", (double)p->weight); rows[n].k = "Peso";   rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%d %%", (int)p->health);    rows[n].k = "Salud";    rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%d %%", (int)p->wool);      rows[n].k = "Lana";     rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%u", (unsigned)p->coins);   rows[n].k = "Monedas";  rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%u", (unsigned)p->best_score); rows[n].k = "Récord"; rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%u", (unsigned)p->care_mistakes); rows[n].k = "Descuidos"; rows[n].v = b[n]; n++;
    snprintf(b[n], 32, "%u / %u / %u", (unsigned)p->times_fed, (unsigned)p->times_played,
             (unsigned)p->times_cleaned);
    rows[n].k = "Com/Jug/Lim"; rows[n].v = b[n]; n++;

    for (int i = 0; i < n; i++) {
        int y = 80 + i * 15;
        g2d_text(t, 26, y, 1, g3d_rgb(170, 166, 190), rows[i].k);
        int w = g2d_text_width(1, rows[i].v);
        g2d_text(t, SCR_W - 26 - w, y, 1, C_WHITE, rows[i].v);
    }
    (void)v;
    g2d_text_center(t, SCR_W / 2, 236, 1, g3d_rgb(160, 156, 176), "tocá para volver");
}

/* ------------------------------------------------------------------ cartel */

void ui_draw_toast(llama_game *g, g3d_target *t)
{
    if (g->toast_t <= 0.f) return;
    int w = g2d_text_width(1, g->toast) + 18;
    if (w > SCR_W - 12) w = SCR_W - 12;
    int x = (SCR_W - w) / 2;
    int y = BAR_Y - 26;
    uint8_t a = (uint8_t)(g->toast_t > 1.f ? 210 : g->toast_t * 210.f);
    g2d_blend_rect(t, x, y, w, 18, C_INK, a);
    g2d_text_center(t, SCR_W / 2, y + 6, 1, C_WHITE, g->toast);
}

/* -------------------------------------------------------------- pantallas */

void ui_draw_boot(llama_game *g, g3d_target *t)
{
    if (g->screen == SCREEN_NEWBORN) {
        g2d_blend_rect(t, 0, 30, SCR_W, 56, C_INK, 180);
        g2d_text_center(t, SCR_W / 2, 38, 2, C_CREAM, "¡NACIÓ!");
        g2d_text_center(t, SCR_W / 2, 62, 1, C_WHITE, "Se llama");
        g2d_text_center(t, SCR_W / 2, 72, 2, C_COIN, g->pet.name);

        g2d_fill_round_rect(t, 14, 236, 100, 34, 8, C_PANEL);
        g2d_text_center(t, 64, 248, 1, C_WHITE, "Otro nombre");
        g2d_fill_round_rect(t, 126, 236, 100, 34, 8, C_ACCENT);
        g2d_text_center(t, 176, 248, 1, C_WHITE, "¡Empezar!");
        return;
    }

    g2d_blend_rect(t, 0, 24, SCR_W, 46, C_INK, 170);
    g2d_text_center(t, SCR_W / 2, 30, 2, C_CREAM, "MI LLAMA");
    g2d_text_center(t, SCR_W / 2, 52, 1, C_WHITE, "mascota virtual 3D");

    if (fmodf(g->screen_t, 1.2f) < 0.8f) {
        g2d_text_center(t, SCR_W / 2, SCR_H - 40, 1, C_CREAM, "tocá la pantalla");
    }
    if (llama_pet_needs_attention(&g->pet)) {
        g2d_text_center(t, SCR_W / 2, SCR_H - 24, 1, C_BAD, "¡te necesita!");
    }
}

void ui_draw_dead(llama_game *g, g3d_target *t)
{
    const llama_pet *p = &g->pet;
    g2d_blend_rect(t, 0, 0, SCR_W, SCR_H, g3d_rgb(10, 10, 24), 140);
    g2d_fill_round_rect(t, 16, 60, SCR_W - 32, 140, 10, C_PANEL);
    g2d_round_rect(t, 16, 60, SCR_W - 32, 140, 10, C_PANEL_LT);
    g2d_text_center(t, SCR_W / 2, 72, 1, C_CREAM, "Se fue al altiplano");

    char line[64], age[24];
    fmt_age(p->age_s, age, sizeof(age));
    snprintf(line, sizeof(line), "%s vivió %s", p->name, age);
    g2d_text_center(t, SCR_W / 2, 96, 1, C_WHITE, line);
    snprintf(line, sizeof(line), "Descuidos: %u", (unsigned)p->care_mistakes);
    g2d_text_center(t, SCR_W / 2, 112, 1, g3d_rgb(180, 176, 196), line);
    snprintf(line, sizeof(line), "Récord de salto: %u", (unsigned)p->best_score);
    g2d_text_center(t, SCR_W / 2, 126, 1, g3d_rgb(180, 176, 196), line);
    g2d_text_center(t, SCR_W / 2, 150, 1, C_CREAM, "Gracias por cuidarla");

    g2d_fill_round_rect(t, 40, 226, SCR_W - 80, 36, 8, C_ACCENT);
    g2d_text_center(t, SCR_W / 2, 239, 1, C_WHITE, "Criar una nueva llama");
}

void ui_draw_minigame(llama_game *g, g3d_target *t)
{
    const minigame_state *m = &g->mg;
    char line[32];

    g2d_blend_rect(t, 0, 0, SCR_W, 26, C_INK, 170);
    snprintf(line, sizeof(line), "Puntos %d", m->score);
    g2d_text(t, 6, 9, 1, C_WHITE, line);
    for (int i = 0; i < 3; i++) {
        g3d_color c = (i < m->lives) ? C_BAD : g3d_rgb(80, 76, 92);
        g2d_fill_circle(t, 118 + i * 14, 12, 4, c);
    }
    g2d_fill_round_rect(t, SCR_W - 30, 4, 24, 18, 4, C_PANEL);
    g2d_text_center(t, SCR_W - 18, 10, 1, C_WHITE, "X");

    if (m->t < 2.2f && m->running) {
        g2d_text_center(t, SCR_W / 2, SCR_H / 2 + 40, 1, C_WHITE, "tocá para saltar");
    }
    if (!m->running) {
        g2d_blend_rect(t, 0, 90, SCR_W, 80, C_INK, 190);
        g2d_text_center(t, SCR_W / 2, 104, 2, C_CREAM, "¡FIN!");
        snprintf(line, sizeof(line), "Puntos: %d", m->score);
        g2d_text_center(t, SCR_W / 2, 130, 1, C_WHITE, line);
        snprintf(line, sizeof(line), "Récord: %u", (unsigned)g->pet.best_score);
        g2d_text_center(t, SCR_W / 2, 146, 1, C_COIN, line);
    }
}
