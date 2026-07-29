/*
 * Estado interno compartido entre game.c, ui.c y minigame.c.
 */
#ifndef GAME_INTERNAL_H
#define GAME_INTERNAL_H

#include "llama_game.h"
#include "llama_model.h"
#include "llama_scene.h"
#include "g2d.h"

/* --------------------------------------------------------------- pantalla */

#define SCR_W 240
#define SCR_H 284

#define HUD_H     44
#define BAR_H     46          /* barra de botones */
#define BTN_COUNT 5

typedef enum {
    SCREEN_BOOT = 0,
    SCREEN_MAIN,
    SCREEN_FOOD,
    SCREEN_STATS,
    SCREEN_MINIGAME,
    SCREEN_DEAD,
    SCREEN_NEWBORN
} game_screen;

typedef enum {
    BTN_COMER = 0,
    BTN_JUGAR,
    BTN_ASEO,
    BTN_DORMIR,
    BTN_SALUD
} game_button;

/* --------------------------------------------------------------- particulas */

typedef enum {
    PT_HEART = 0,
    PT_ZZZ,
    PT_STAR,
    PT_SPIT,
    PT_CRUMB,
    PT_ANGRY,
    PT_COIN,
    PT_NOTE,
    PT_COUNT
} particle_kind;

typedef struct {
    g3d_v3 pos, vel;
    float  life, life0;
    float  size;
    uint8_t kind;
    bool   used;
} particle;

#define MAX_PARTICLES 20

/* ---------------------------------------------------------------- minijuego */

#define MG_OBSTACLES 4

typedef struct {
    bool   running;
    float  t;
    float  speed;
    float  llama_z;       /* posicion lateral fija; corre sobre el eje X */
    float  jump_y;
    float  jump_v;
    bool   jumping;
    float  obst_x[MG_OBSTACLES];
    bool   obst_alive[MG_OBSTACLES];
    int    score;
    int    lives;
    float  hit_flash;
    float  end_timer;
} minigame_state;

/* ------------------------------------------------------------------- juego */

struct llama_game {
    int         w, h;
    llama_pet   pet;
    llama_model model;
    llama_scene scene;
    g3d_ctx     ctx;

    game_screen screen;
    float       screen_t;

    /* Camara orbital. */
    float cam_yaw, cam_yaw_vel;
    float cam_dist, cam_height, cam_target_y;
    float cam_shake;

    /* Tactil. */
    bool     touch_prev;
    int      press_x, press_y, last_x, last_y;
    uint32_t press_ms;
    bool     dragging;
    int      pressed_btn;        /* boton bajo el dedo, -1 si ninguno */
    bool     tap_pending;
    int      tap_x, tap_y;

    /* Comportamiento de la llama. */
    float      wander_timer;
    float      wander_target_x, wander_target_z;
    float      action_timer;     /* mientras > 0 manda la animacion de accion */
    llama_anim action_anim;
    float      bowl_timer;       /* comedero visible */
    int        bowl_food;        /* llama_food */
    float      ball_timer;
    g3d_v3     ball_pos;
    g3d_v3     ball_vel;

    /* Efectos. */
    particle particles[MAX_PARTICLES];
    char     toast[48];
    float    toast_t;
    float    flash;              /* destello blanco al accionar */

    /* IMU. */
    float shake_energy;
    float tilt_x, tilt_y;
    float last_ax, last_ay, last_az;

    /* Sistema. */
    float  save_timer;
    double last_time;
    int    battery_pct;
    bool   charging;
    float  night;                /* 0 = dia, 1 = noche */
    int    fps;
    float  fps_acc;
    int    fps_frames;

    minigame_state mg;
};

/* ----------------------------------------------------------------- helpers */

void  game_toast(llama_game *g, const char *msg);
void  game_emit(llama_game *g, particle_kind kind, g3d_v3 pos, int count);
void  game_particles_update(llama_game *g, float dt);
void  game_particles_draw(llama_game *g, g3d_target *t);

/* ui.c */
void  ui_draw_hud(llama_game *g, g3d_target *t);
void  ui_draw_buttons(llama_game *g, g3d_target *t);
void  ui_draw_food_menu(llama_game *g, g3d_target *t);
void  ui_draw_stats(llama_game *g, g3d_target *t);
void  ui_draw_toast(llama_game *g, g3d_target *t);
void  ui_draw_boot(llama_game *g, g3d_target *t);
void  ui_draw_dead(llama_game *g, g3d_target *t);
void  ui_draw_minigame(llama_game *g, g3d_target *t);
void  ui_draw_icon(g3d_target *t, int kind, int cx, int cy, int size, g3d_color col);
int   ui_button_at(int x, int y);          /* -1 si el toque no cae en un boton */
bool  ui_shear_button_hit(const llama_game *g, int x, int y);

/* minigame.c */
void  minigame_start(llama_game *g);
void  minigame_update(llama_game *g, float dt, bool tap);
void  minigame_draw(llama_game *g, g3d_target *t);

#endif /* GAME_INTERNAL_H */
